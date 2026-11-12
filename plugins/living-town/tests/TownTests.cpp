#include <Town.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <numeric>
#include <set>

using namespace umbra;
using namespace umbra::town;

namespace
{
    const std::filesystem::path assetDir = UMBRA_TEST_ASSET_DIRECTORY;

    // The reference town, checked against the reference RPG content.
    TownDef Settlement()
    {
        std::vector<core::Diagnostic> diagnostics;
        rpg::Content content;
        REQUIRE(rpg::LoadContent(assetDir / "rpg/reference.rpg.json", content, diagnostics));
        TownDef def;
        REQUIRE(LoadTown(assetDir / "town/settlement.town.json", &content, def, diagnostics));
        REQUIRE(diagnostics.empty());
        return def;
    }

    bool Has(const std::vector<core::Diagnostic> &diagnostics, const std::string &code)
    {
        return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const core::Diagnostic &d) { return d.code == code; });
    }

    std::vector<core::Diagnostic> Parse(const std::string &text, const rpg::Content *content = nullptr)
    {
        TownDef def;
        std::vector<core::Diagnostic> diagnostics;
        ParseTown(text, "test.town.json", content, def, diagnostics);
        return diagnostics;
    }

    long long Money(const TownState &state)
    {
        return std::accumulate(state.residents.begin(), state.residents.end(), 0LL, [](long long sum, const ResidentState &r) { return sum + r.money; });
    }

    constexpr int week = 7 * ticksPerDay;
}

TEST_CASE("The reference town loads: seven residents with homes, work and a plan (T90)", "[town]")
{
    const TownDef def = Settlement();
    CHECK(def.residents.size() == 7);
    CHECK(def.needs.size() == 3);
    CHECK(def.places[static_cast<std::size_t>(def.Place("inn"))].owner == def.Resident("edda"));
    CHECK(def.activities[0].id == "rest");
    CHECK(def.fingerprint != 0);

    const TownState start = NewTown(def, 1);
    CHECK(start.Day() == 1);
    CHECK(start.Hour() == 0);
    for (std::size_t i = 0; i < def.residents.size(); ++i)
        CHECK(start.residents[i].place == def.residents[i].home);
    CHECK(start.Bond(static_cast<std::size_t>(def.Resident("bren")), static_cast<std::size_t>(def.Resident("pip"))) == 400);
}

TEST_CASE("Mistakes in a town file are named by field (T90)", "[town]")
{
    const std::string head = R"({"format": "umbra.town", "version": 1, )";
    const std::string needs = R"("needs": [{"id": "hunger", "risePerDay": 1000}], )";
    const std::string places = R"("places": [{"id": "home"}], )";
    const std::string rest = R"({"id": "rest", "where": "home"})";
    const std::string people = R"("residents": [{"id": "a", "home": "home"}]})";
    CHECK(Parse(head + needs + places + R"("activities": [)" + rest + "], " + people).empty());
    CHECK(Has(Parse(R"({"format": "umbra.scene"})"), "town.format"));
    CHECK(Has(Parse(head + places + R"("activities": [)" + rest + "], " + people), "town.needs"));
    CHECK(Has(Parse(head + needs + R"("activities": [)" + rest + "], " + people), "town.places"));
    CHECK(Has(Parse(head + needs + places + R"("activities": [{"id": "rest", "where": "home", "cost": 2}], )" + people), "town.activities"));
    CHECK(Has(Parse(head + needs + places + R"("activities": [)" + rest + R"(, {"id": "x", "where": "moon"}], )" + people), "town.where"));
    CHECK(Has(Parse(head + needs + places + R"("activities": [)" + rest + R"(, {"id": "x", "where": "home", "relief": {"thirst": 5}}], )" + people), "town.relief"));
    CHECK(Has(Parse(head + needs + places + R"("activities": [)" + rest + R"(, {"id": "rest", "where": "home"}], )" + people), "town.id"));
    CHECK(Has(Parse(head + needs + places + R"("activities": [)" + rest + R"(], "residents": [{"id": "a", "home": "castle"}]})"), "town.home"));
    CHECK(Has(Parse(head + needs + places + R"("activities": [)" + rest + R"(], "residents": [{"id": "a", "home": "home", "schedule": [{"activity": "fly"}]}]})"),
              "town.schedule"));
    CHECK(Has(Parse(head + needs + places + R"("activities": [)" + rest + R"(], "residents": [{"id": "a", "home": "home", "bonds": {"ghost": 5}}]})"), "town.bonds"));
    CHECK(Has(Parse(head + needs + R"("places": [{"id": "home", "owner": "ghost"}], "activities": [)" + rest + "], " + people), "town.owner"));
    CHECK(Has(Parse(head + needs + places + R"("activities": [)" + rest + R"(], "favours": [{"id": "f", "resident": "a", "target": "a", "ask": "x", "thanks": "y"}], )" + people.substr(0, people.size() - 1) + "}"),
              "town.favour"));
    // With RPG content, a resident must be one of its actors.
    std::vector<core::Diagnostic> diagnostics;
    rpg::Content content;
    REQUIRE(rpg::LoadContent(assetDir / "rpg/reference.rpg.json", content, diagnostics));
    CHECK(Has(Parse(head + needs + places + R"("activities": [)" + rest + R"(], "residents": [{"id": "a", "home": "home", "actor": "dragon"}]})", &content), "town.actor"));
}

TEST_CASE("The same seed gives the same week on every compiler; another seed another week (T90)", "[town]")
{
    const TownDef def = Settlement();
    TownState a = NewTown(def, 1), b = NewTown(def, 1), c = NewTown(def, 2);
    Run(def, a, week);
    Run(def, b, week);
    Run(def, c, week);
    CHECK(Fingerprint(a) == Fingerprint(b));
    CHECK(Fingerprint(a) != Fingerprint(c));
    CHECK(a.Day() == 8);
    // The golden number: GCC, Clang and MSVC must all get it (integer rules, the town's own dice).
    // It changes when the rules or town/settlement.town.json change: then run
    // `UmbraTool town-run <town file>` and put the fingerprint it prints here.
    CHECK(Fingerprint(a) == 11171925813610364305ULL);
}

TEST_CASE("A fork run forward equals the original run forward, and never changes it (T90)", "[town]")
{
    const TownDef def = Settlement();
    TownState world = NewTown(def, 7);
    Run(def, world, 3 * ticksPerDay);
    const std::uint64_t before = Fingerprint(world);

    // 1. Rehearse two days in a copy: the real world does not move.
    TownState fork = world;
    Run(def, fork, 2 * ticksPerDay);
    CHECK(Fingerprint(world) == before);
    CHECK(fork.tick == world.tick + 2 * ticksPerDay);

    // 2. The real world then lives the same two days and arrives at the same place.
    Run(def, world, 2 * ticksPerDay);
    CHECK(Fingerprint(world) == Fingerprint(fork));

    // 3. A fork that is nudged (someone made to do something else) goes its own way.
    TownState nudged = world, plain = world;
    for (int tick = 0; tick < ticksPerDay; ++tick)
    {
        const int pip = def.Resident("pip");
        if (nudged.residents[static_cast<std::size_t>(pip)].until <= nudged.tick)
            Apply(def, nudged, {pip, def.Activity("chat")});
        Tick(def, nudged);
        Tick(def, plain);
    }
    CHECK(Fingerprint(nudged) != Fingerprint(plain));
}

TEST_CASE("Only legal intents change the town (T90)", "[town]")
{
    const TownDef def = Settlement();
    TownState state = NewTown(def, 3);
    const int pip = def.Resident("pip"), edda = def.Resident("edda");
    const std::uint64_t before = Fingerprint(state);

    // Midnight: the inn's kitchen is closed, nobody of that name lives here, no such activity.
    CHECK(Apply(def, state, {pip, def.Activity("eat_inn")}) == Refusal::Closed);
    CHECK(Apply(def, state, {99, 0}) == Refusal::UnknownResident);
    CHECK(Apply(def, state, {pip, 99}) == Refusal::UnknownActivity);
    CHECK(Fingerprint(state) == before);

    // Noon, after a free rest each hour: Pip has 5 coins, a meal at the inn costs 6.
    while (state.Hour() != 12)
    {
        for (int i = 0; i < 7; ++i)
            Apply(def, state, {i, 0});
        Tick(def, state);
    }
    state.residents[static_cast<std::size_t>(pip)].until = state.tick; // free now
    state.residents[static_cast<std::size_t>(pip)].money = 5;
    CHECK(Apply(def, state, {pip, def.Activity("eat_inn")}) == Refusal::CannotPay);
    // With 6 coins it works: Pip is at the inn, busy for an hour, and Edda (the owner) has the coins.
    state.residents[static_cast<std::size_t>(pip)].money = 6;
    const int eddaBefore = state.residents[static_cast<std::size_t>(edda)].money;
    CHECK(Apply(def, state, {pip, def.Activity("eat_inn")}) == Refusal::None);
    CHECK(state.residents[static_cast<std::size_t>(pip)].place == def.Place("inn"));
    CHECK(state.residents[static_cast<std::size_t>(pip)].money == 0);
    CHECK(state.residents[static_cast<std::size_t>(edda)].money == eddaBefore + 6);
    CHECK(Apply(def, state, {pip, 0}) == Refusal::Busy);
    // The owner eats at her own inn for nothing.
    state.residents[static_cast<std::size_t>(edda)].until = state.tick;
    state.residents[static_cast<std::size_t>(edda)].money = 0;
    CHECK(Apply(def, state, {edda, def.Activity("eat_inn")}) == Refusal::None);

    // Every intent the town offers is one it accepts.
    for (int i = 0; i < 7; ++i)
        for (const Intent &intent : LegalIntents(def, state, i))
            CHECK(Legal(def, state, intent) == Refusal::None);
}

TEST_CASE("A month in ten towns: needs, places and money always add up (T90)", "[town]")
{
    const TownDef def = Settlement();
    std::set<std::uint64_t> endings;
    for (std::uint64_t seed = 1; seed <= 10; ++seed)
    {
        TownState state = NewTown(def, seed);
        const long long start = Money(state);
        int worked = 0, slept = 0, crises = 0;
        for (int tick = 0; tick < 30 * ticksPerDay; ++tick)
        {
            Tick(def, state);
            for (std::size_t i = 0; i < state.residents.size(); ++i)
            {
                const ResidentState &r = state.residents[i];
                REQUIRE(r.place >= 0);
                REQUIRE(static_cast<std::size_t>(r.place) < def.places.size());
                REQUIRE(r.money >= 0);
                for (std::size_t n = 0; n < def.needs.size(); ++n)
                    REQUIRE((state.Need(i, n) >= 0 && state.Need(i, n) <= needFull));
                worked += r.activity == def.Activity("work");
                slept += r.activity == def.Activity("sleep");
            }
            // Coins only appear as wages and only vanish where nobody owns the place.
            REQUIRE(Money(state) == start + state.minted);
        }
        for (const TownEvent &e : state.events)
            crises += e.kind == EventKind::Crisis;
        INFO("seed " << seed);
        CHECK(worked > 30 * ticksPerDay); // together more than one resident-month of work...
        CHECK(slept > 30 * ticksPerDay);  // ...and of sleep: the schedules are lived
        CHECK(crises < 40);               // and nobody starves through the month
        CHECK(state.events.size() <= TownState::maxEvents);
        endings.insert(Fingerprint(state));
    }
    CHECK(endings.size() == 10);
}

TEST_CASE("A saved town continues exactly; a save from another town is refused (T90)", "[town]")
{
    const TownDef def = Settlement();
    TownState state = NewTown(def, 5);
    Run(def, state, 2 * ticksPerDay + 37);

    // 1. Through text and back: the same state, and the same future.
    const std::string text = SaveTown(def, state).dump();
    TownState loaded;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(LoadTownState(def, nlohmann::json::parse(text), loaded, diagnostics));
    CHECK(Fingerprint(loaded) == Fingerprint(state));
    Run(def, state, week);
    Run(def, loaded, week);
    CHECK(Fingerprint(loaded) == Fingerprint(state));

    // 2. A town file whose ids changed does not accept the save; a damaged save changes nothing.
    TownDef other = def;
    other.fingerprint += 1;
    TownState untouched = NewTown(def, 9);
    const std::uint64_t before = Fingerprint(untouched);
    CHECK_FALSE(LoadTownState(other, nlohmann::json::parse(text), untouched, diagnostics));
    CHECK(Has(diagnostics, "town.state.town"));
    nlohmann::json damaged = nlohmann::json::parse(text);
    damaged["residents"][2]["place"] = "the moon";
    CHECK_FALSE(LoadTownState(def, damaged, untouched, diagnostics));
    CHECK(Has(diagnostics, "town.state.resident"));
    damaged = nlohmann::json::parse(text);
    damaged.erase("tick");
    CHECK_FALSE(LoadTownState(def, damaged, untouched, diagnostics));
    CHECK(Has(diagnostics, "town.state.read"));
    CHECK_FALSE(LoadTownState(def, nlohmann::json::parse(R"({"format": "umbra.save"})"), untouched, diagnostics));
    CHECK(Fingerprint(untouched) == before);
}

TEST_CASE("A proposal for a free resident is used if legal, else the rule-based choice (T90)", "[town]")
{
    const TownDef def = Settlement();
    struct Ask
    {
        int activity, asked = 0;
    };
    const Decider decide = [](const TownDef &, const TownState &, int resident, void *user) -> std::optional<Intent> {
        ++static_cast<Ask *>(user)->asked;
        return Intent{resident, static_cast<Ask *>(user)->activity};
    };
    // 1. Legal: at 08:00 everyone asked to talk at the well goes there.
    TownState state = NewTown(def, 4);
    Run(def, state, 8 * ticksPerHour);
    for (ResidentState &r : state.residents)
        r.until = state.tick;
    Ask chat{def.Activity("chat")};
    Tick(def, state, decide, &chat);
    CHECK(chat.asked == 7);
    for (const ResidentState &r : state.residents)
        CHECK(r.place == def.Place("well"));

    // 2. Illegal (no such activity): the town runs exactly as it would with nobody asking.
    TownState asked = NewTown(def, 4), alone = NewTown(def, 4);
    Ask nonsense{999};
    for (int tick = 0; tick < 2 * ticksPerDay; ++tick)
    {
        Tick(def, asked, decide, &nonsense);
        Tick(def, alone);
    }
    CHECK(nonsense.asked > 100);
    CHECK(Fingerprint(asked) == Fingerprint(alone));
}

TEST_CASE("Fast-forward: a year of the town in well under a second (T90)", "[town]")
{
    const TownDef def = Settlement();
    TownState state = NewTown(def, 11);
    const auto start = std::chrono::steady_clock::now();
    Run(def, state, 365 * ticksPerDay);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    WARN("365 days of seven residents: " << seconds << " s (" << static_cast<int>(365.0 / seconds) << " days per second, " << sizeof(ResidentState) * 7 << " bytes of residents)");
    CHECK(state.Day() == 366);
    CHECK(seconds < 5.0); // generous: debug and sanitizer builds too
}

// ---------------- S22: memory, rumours, the chronicle (T91) ----------------

namespace
{
    std::uint64_t Bit(int resident) { return 1ULL << resident; }
}

TEST_CASE("Events are known only to those who were there, then travel by talk (T91)", "[town]")
{
    const TownDef def = Settlement();
    TownState state = NewTown(def, 1);
    Run(def, state, 30 * ticksPerDay);
    REQUIRE_FALSE(state.facts.empty());
    REQUIRE(state.facts.size() <= TownState::maxFacts);
    int travelled = 0;
    for (const Fact &f : state.facts)
    {
        CHECK((f.saw & Bit(f.who)) != 0);     // whoever it is about was there
        CHECK((f.saw & ~f.knows) == 0);       // everyone who saw it knows it
        CHECK(f.told == 0);                   // nobody has talked to the player
        travelled += f.knows != f.saw;
    }
    CHECK(travelled > 5); // rumours do spread...
    // ...in order of time, and never to someone who neither saw it nor talked to anyone.
    for (std::size_t i = 1; i < state.facts.size(); ++i)
        CHECK(state.facts[i - 1].tick <= state.facts[i].tick);

    // A town where nobody ever meets: every fact stays with its witnesses.
    TownDef alone = def;
    for (ActivityDef &a : alone.activities)
        a.social = false;
    TownState quiet = NewTown(alone, 1);
    Run(alone, quiet, 30 * ticksPerDay);
    REQUIRE_FALSE(quiet.facts.empty());
    for (const Fact &f : quiet.facts)
        CHECK(f.knows == f.saw);
}

TEST_CASE("A skipped favour: the asker knows at once, others hear of it and say so (T91)", "[town]")
{
    const TownDef def = Settlement();
    const int edda = def.Resident("edda"), bren = def.Resident("bren");
    TownState state = NewTown(def, 1);
    Run(def, state, 12 * ticksPerHour);

    // 1. Edda asks; the player does nothing for a day and a bit.
    CHECK(TalkTo(def, state, edda).find("Look in on her") != std::string::npos);
    CHECK(state.favours[0].status == FavourStatus::Asked);
    Run(def, state, 25 * ticksPerHour);
    CHECK(state.favours[0].status == FavourStatus::Skipped);
    const auto skipped = [&]() -> const Fact & { return *std::find_if(state.facts.begin(), state.facts.end(), [](const Fact &f) { return f.kind == FactKind::FavourSkipped; }); };
    CHECK(skipped().saw == Bit(edda)); // only she knows...
    CHECK(state.regard[static_cast<std::size_t>(edda)] == -120);
    CHECK(TalkTo(def, state, edda).find("You never went") != std::string::npos); // ...and says so first

    // 2. Two days later someone else has heard, thinks less of the player, and mentions it.
    Run(def, state, 2 * ticksPerDay);
    const std::uint64_t others = skipped().knows & ~Bit(edda);
    REQUIRE(others != 0);
    int heard = 0;
    while (!(others & Bit(heard)))
        ++heard;
    CHECK(state.regard[static_cast<std::size_t>(heard)] == -60);
    CHECK(TalkTo(def, state, heard) == def.residents[static_cast<std::size_t>(heard)].name + ": They say you let Edda down: you never went to see Old Bren.");
    // Someone who has not heard is as friendly as before.
    for (int i = 0; i < 7; ++i)
        if (!(skipped().knows & Bit(i)))
            CHECK(state.regard[static_cast<std::size_t>(i)] == 0);
    (void)bren;

    // 3. The chronicle holds what the player was told, oldest first, each once.
    const std::vector<std::string> lines = Chronicle(def, state);
    REQUIRE_FALSE(lines.empty());
    CHECK(std::count_if(lines.begin(), lines.end(), [](const std::string &l) { return l.find("You let Edda down") != std::string::npos; }) == 1);
    CHECK(Chronicle(def, state) == lines); // reading it changes nothing
}

TEST_CASE("A kept favour: thanks on the spot, the asker hears and warms to the player (T91)", "[town]")
{
    const TownDef def = Settlement();
    const int edda = def.Resident("edda"), bren = def.Resident("bren");
    TownState state = NewTown(def, 2);
    Run(def, state, 12 * ticksPerHour);
    (void)TalkTo(def, state, edda);
    CHECK(TalkTo(def, state, bren).find("Kind of you to come") != std::string::npos);
    CHECK(state.favours[0].status == FavourStatus::Kept);
    CHECK(state.regard[static_cast<std::size_t>(bren)] == 40);
    CHECK(Chronicle(def, state).back().find("You kept your word to Edda") != std::string::npos);
    // The deadline passing changes nothing now; a week later Edda has heard.
    Run(def, state, 7 * ticksPerDay);
    CHECK(state.favours[0].status == FavourStatus::Kept);
    const Fact &kept = *std::find_if(state.facts.begin(), state.facts.end(), [](const Fact &f) { return f.kind == FactKind::FavourKept; });
    CHECK((kept.knows & Bit(edda)) != 0);
    CHECK(state.regard[static_cast<std::size_t>(edda)] == 80);
    // With nothing new to tell, a resident greets by what they think of the player.
    std::string last;
    for (int i = 0; i < 200 && last.find("Good to see you!") == std::string::npos; ++i)
        last = TalkTo(def, state, edda);
    CHECK(last.find("Good to see you! I'm ") != std::string::npos);
}

TEST_CASE("Every kind of fact has a chronicle sentence and a spoken line; memories are saved (T91)", "[town]")
{
    const TownDef def = Settlement();
    for (const FactKind kind : {FactKind::LevelUp, FactKind::Crisis, FactKind::Friends, FactKind::Rivals, FactKind::FavourKept, FactKind::FavourSkipped})
    {
        const Fact fact{0, kind, 1, 2, 0, Bit(1), Bit(1) | Bit(3), 0};
        CHECK_FALSE(Sentence(def, fact).empty());
        CHECK_FALSE(Says(def, fact, 1).empty());                          // about themselves
        CHECK(Says(def, fact, 3).starts_with("They say "));               // heard
        CHECK(Says(def, Fact{0, kind, 1, 2, 0, Bit(3), Bit(3), 0}, 3).starts_with("I saw it myself: ")); // seen
    }
    // Memories, regard and favours go through a save and continue the same.
    TownState state = NewTown(def, 6);
    Run(def, state, 5 * ticksPerDay);
    (void)TalkTo(def, state, def.Resident("edda"));
    (void)TalkTo(def, state, def.Resident("garrick"));
    Run(def, state, 2 * ticksPerDay);
    for (int i = 0; i < 7; ++i)
        (void)TalkTo(def, state, i);
    TownState loaded;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(LoadTownState(def, nlohmann::json::parse(SaveTown(def, state).dump()), loaded, diagnostics));
    CHECK(Fingerprint(loaded) == Fingerprint(state));
    CHECK(Chronicle(def, loaded) == Chronicle(def, state));
    // A save from before S22 (no memories) still loads: nobody remembers anything.
    nlohmann::json old = SaveTown(def, state);
    old.erase("facts"), old.erase("regard"), old.erase("favours");
    REQUIRE(LoadTownState(def, old, loaded, diagnostics));
    CHECK(loaded.facts.empty());
    CHECK(loaded.regard == std::vector<std::int16_t>(7, 0));
}
