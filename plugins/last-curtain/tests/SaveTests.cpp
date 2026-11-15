#include "NightTestData.hpp"

#include <NightSave.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace umbra::night;

namespace
{
    // A fixed chooser: a random legal action from its own generator; EndTurn now and then so no turn runs forever.
    struct Chooser
    {
        std::uint64_t seed;
        int steps = 0;
        Action Pick(const NightDef &def, const NightState &state)
        {
            const auto legal = LegalActions(def, state);
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            const auto end = std::find(legal.begin(), legal.end(), Action{ActionKind::EndTurn});
            if (++steps % 6 == 0 && end != legal.end())
                return *end;
            return legal[(seed >> 33) % legal.size()];
        }
    };

    // Drives the night for at most `count` Applies; returns how many were made.
    int Drive(const NightDef &def, NightState &state, Chooser &chooser, int count)
    {
        int n = 0;
        while (n < count && Current(state) >= 0)
        {
            REQUIRE(Apply(def, state, chooser.Pick(def, state)));
            ++n;
        }
        return n;
    }

    NightState RoundTrip(const NightDef &def, const NightState &state)
    {
        const nlohmann::json saved = nlohmann::json::parse(SaveNight(def, state).dump());
        std::vector<umbra::core::Diagnostic> diagnostics;
        NightState loaded;
        const bool ok = LoadNightState(def, saved, loaded, diagnostics);
        INFO((diagnostics.empty() ? std::string() : diagnostics.front().message));
        REQUIRE(ok);
        return loaded;
    }
}

TEST_CASE("Same data and seed, same night (T103)", "[night][save]")
{
    const NightDef def = Data();
    NightState a = NewNight(def, 1), b = NewNight(def, 1);
    Chooser ca{1}, cb{1};
    Drive(def, a, ca, 5000);
    Drive(def, b, cb, 5000);
    CHECK(Fingerprint(a) == Fingerprint(b));
    CHECK(Fingerprint(a) != Fingerprint(NewNight(def, 1)));
    // Golden: update when the data or the rules change (Night.md "Fingerprint").
    CHECK(Fingerprint(a) == 0x7fdfd2e99239e03dULL);
}

TEST_CASE("A save at any point continues to the same end (T103)", "[night][save]")
{
    const NightDef def = Data();
    bool midBattle = false, afterTurn = false, pendingLevel = false;
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        for (int cut : {37, 150, 400, 700})
        {
            NightState state = FoyerNight(def, seed);
            Chooser chooser{seed};
            Drive(def, state, chooser, cut + static_cast<int>(seed));
            midBattle = midBattle || state.battle.active;
            afterTurn = afterTurn || std::any_of(state.guests.begin(), state.guests.end(), [](const Guest &g) { return g.ghost; });
            for (const Guest &g : state.guests)
                pendingLevel = pendingLevel || g.pending > 0;
            NightState loaded = RoundTrip(def, state);
            CHECK(Fingerprint(loaded) == Fingerprint(state));
            Chooser again = chooser;
            Drive(def, state, chooser, 5000);
            Drive(def, loaded, again, 5000);
            CHECK(Fingerprint(loaded) == Fingerprint(state));
        }
    }
    CHECK(midBattle);
    CHECK(afterTurn);

    // A pending level: with one kill per level, save the moment one is pending.
    NightDef quick = def;
    quick.rules.levels.xp = {1, 2, 3, 4};
    quick.rules.monsters.chancePercent = 100;
    for (std::uint64_t seed = 1; seed <= 50 && !pendingLevel; ++seed)
    {
        NightState state = FoyerNight(quick, seed);
        Chooser chooser{seed};
        while (Current(state) >= 0 && !pendingLevel)
        {
            Drive(quick, state, chooser, 1);
            for (const Guest &g : state.guests)
                pendingLevel = pendingLevel || g.pending > 0;
        }
        if (!pendingLevel)
            continue;
        NightState loaded = RoundTrip(quick, state);
        Chooser again = chooser;
        Drive(quick, state, chooser, 5000);
        Drive(quick, loaded, again, 5000);
        CHECK(Fingerprint(loaded) == Fingerprint(state));
    }
    CHECK(pendingLevel);
}

TEST_CASE("A damaged save or one from other data is refused (T103)", "[night][save]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 3);
    Chooser chooser{3};
    Drive(def, state, chooser, 100);
    const nlohmann::json good = SaveNight(def, state);
    auto refused = [&](const NightDef &with, const nlohmann::json &saved) {
        std::vector<umbra::core::Diagnostic> diagnostics;
        NightState out;
        CHECK_FALSE(LoadNightState(with, saved, out, diagnostics));
        REQUIRE_FALSE(diagnostics.empty());
        CHECK(diagnostics.front().code == "night.save");
    };
    nlohmann::json missing = good;
    missing["state"].erase("guests");
    refused(def, missing);
    nlohmann::json wrong = good;
    wrong["state"]["guests"][0]["cell"] = 99;
    refused(def, wrong);
    refused(def, nlohmann::json::parse("[1, 2]"));

    // Other data: one number changed in a copy of the folder.
    const std::filesystem::path folder = std::filesystem::temp_directory_path() / "umbra-night-other";
    std::filesystem::remove_all(folder);
    std::filesystem::copy(UMBRA_NIGHT_DATA, folder);
    {
        std::ifstream in(folder / "rules.json");
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        text.replace(text.find("\"carry\": 4"), 10, "\"carry\": 3");
        std::ofstream(folder / "rules.json", std::ios::trunc) << text;
    }
    std::vector<umbra::core::Diagnostic> diagnostics;
    NightDef other;
    REQUIRE(LoadNight(folder, other, diagnostics));
    CHECK(other.fingerprint != def.fingerprint);
    refused(other, good);
}

TEST_CASE("A mid-evening save round-trips goals, groups, approvals and moves (T122, T157)", "[night][save]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 7, 0);
    Chooser chooser{7};
    while (state.round < 4 && Current(state) >= 0)
        Drive(def, state, chooser, 1);
    REQUIRE_FALSE(state.taken.present);
    // Set every new field to something other than its default.
    Guest &g = state.guests[2];
    g.goalCount = 1, g.goalRoomSeen = true, g.knows = 0b101, g.origin = static_cast<std::int8_t>(state.foyer), g.group = 3;
    g.budget = 2, g.steps = 5, g.bonusUsed.set(70);
    state.guests[3].group = 3;
    state.approvals[3 * maxGuests + 2] = 2;
    state.chat = {true, 3, 2, 0};
    state.cells[static_cast<std::size_t>(state.foyer)].event = 4; // an armed event (version 3)
    const NightState loaded = RoundTrip(def, state);
    CHECK(Fingerprint(loaded) == Fingerprint(state));
    const Guest &l = loaded.guests[2];
    CHECK(l.goal == g.goal);
    CHECK(l.goalCount == 1);
    CHECK(l.goalRoomSeen);
    CHECK(l.knows == 0b101);
    CHECK(l.origin == state.foyer);
    CHECK(l.budget == 2);
    CHECK(l.steps == 5);
    CHECK(l.bonusUsed.test(70));
    CHECK(loaded.guests[3].group == 3);
    CHECK(loaded.approvals[3 * maxGuests + 2] == 2);
    CHECK((loaded.chat.asker == 3 && loaded.chat.answerer == 2));
    CHECK(loaded.cells[static_cast<std::size_t>(state.foyer)].event == 4);

    // Out-of-range values in the new fields are refused.
    std::vector<umbra::core::Diagnostic> diagnostics;
    NightState out;
    for (const char *field : {"answerer", "origin", "approvals", "event"})
    {
        nlohmann::json bad = SaveNight(def, state);
        if (std::string(field) == "answerer")
            bad["state"]["chat"]["answerer"] = 9;
        else if (std::string(field) == "origin")
            bad["state"]["guests"][2]["origin"] = 120;
        else if (std::string(field) == "event")
            bad["state"]["cells"][0]["event"] = 99;
        else
            bad["state"]["approvals"][0] = 9;
        INFO(field);
        CHECK_FALSE(LoadNightState(def, bad, out, diagnostics));
    }
}

TEST_CASE("An older save is refused (T122, T146, T157)", "[night][save]")
{
    const NightDef def = Data();
    nlohmann::json saved = SaveNight(def, FoyerNight(def, 3));
    CHECK(saved["version"] == 6);
    saved["version"] = 5;
    std::vector<umbra::core::Diagnostic> diagnostics;
    NightState out;
    CHECK_FALSE(LoadNightState(def, saved, out, diagnostics));
    REQUIRE_FALSE(diagnostics.empty());
    CHECK(diagnostics.front().message.find("version 6") != std::string::npos);
}
