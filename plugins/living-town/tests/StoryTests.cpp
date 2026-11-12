#include <Soak.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>

using namespace umbra;
using namespace umbra::town;

// S24: trouble, the outcome plan, the storyteller and the soak test (T93).
namespace
{
    const std::filesystem::path assetDir = UMBRA_TEST_ASSET_DIRECTORY;

    TownDef Settlement()
    {
        std::vector<core::Diagnostic> diagnostics;
        TownDef def;
        REQUIRE(LoadTown(assetDir / "town/settlement.town.json", nullptr, def, diagnostics));
        return def;
    }
    PlanDef Plan(const TownDef &def, const std::string &body)
    {
        std::vector<core::Diagnostic> diagnostics;
        PlanDef plan;
        const bool ok = ParsePlan(R"({"format": "umbra.town.plan", "version": 1, )" + body + "}", "test", def, plan, diagnostics);
        for (const core::Diagnostic &d : diagnostics)
            UNSCOPED_INFO(d.code << " " << d.field << ": " << d.message);
        REQUIRE(ok);
        return plan;
    }
    std::string Code(const TownDef &def, const std::string &body)
    {
        std::vector<core::Diagnostic> diagnostics;
        PlanDef plan;
        CHECK_FALSE(ParsePlan(R"({"format": "umbra.town.plan", "version": 1, )" + body + "}", "test", def, plan, diagnostics));
        return diagnostics.empty() ? "" : diagnostics.front().code;
    }
    long long Coins(const TownState &state)
    {
        long long coins = 0;
        for (const ResidentState &r : state.residents)
            coins += r.money;
        return coins;
    }
}

TEST_CASE("Trouble does what it says, and only what is possible (T93)", "[story]")
{
    const TownDef def = Settlement();
    const int tobin = def.Resident("tobin"), hale = def.Resident("hale"), edda = def.Resident("edda"), wynn = def.Resident("wynn");
    TownState state = NewTown(def, 4);
    Run(def, state, ticksPerDay + 12 * ticksPerHour); // day 2, noon
    const long long before = Coins(state) - state.minted;

    // 1. Illness: at home for two days, then back to work.
    REQUIRE(Happen(def, state, {TroubleKind::Ill, wynn, -1}));
    CHECK_FALSE(Possible(def, state, {TroubleKind::Ill, wynn, -1})); // already ill
    const int busyUntil = state.residents[static_cast<std::size_t>(wynn)].until; // what she was doing, she finishes
    for (int tick = 0; tick < 2 * ticksPerDay; ++tick)
    {
        Tick(def, state);
        if (state.tick > busyUntil && state.tick < state.residents[static_cast<std::size_t>(wynn)].illUntil)
            CHECK(state.residents[static_cast<std::size_t>(wynn)].place == def.residents[static_cast<std::size_t>(wynn)].home);
    }
    Run(def, state, ticksPerDay);
    CHECK(Possible(def, state, {TroubleKind::Ill, wynn, -1}));

    // 2. Theft: coins move, nothing is made or lost; the victim knows and thinks worse of the thief.
    const int had = state.residents[static_cast<std::size_t>(tobin)].money, feeling = state.Bond(static_cast<std::size_t>(tobin), static_cast<std::size_t>(hale));
    REQUIRE(Happen(def, state, {TroubleKind::Theft, hale, tobin}));
    CHECK(state.residents[static_cast<std::size_t>(tobin)].money < had);
    CHECK(Coins(state) - state.minted == before);
    CHECK(state.Bond(static_cast<std::size_t>(tobin), static_cast<std::size_t>(hale)) == std::max(-1000, feeling - 400));
    CHECK(state.facts.back().kind == FactKind::Theft);
    CHECK((state.facts.back().knows & (1ULL << tobin)) != 0);
    CHECK(Says(def, state.facts.back(), tobin) == "Hale robbed me. My own neighbour.");
    CHECK_FALSE(Possible(def, state, {TroubleKind::Theft, hale, hale}));

    // 3. A row: both think worse of each other; then the town proposes it again or one leaving.
    REQUIRE(Happen(def, state, {TroubleKind::Feud, edda, wynn}));
    CHECK(state.Bond(static_cast<std::size_t>(edda), static_cast<std::size_t>(wynn)) <= -300);
    const std::vector<Trouble> proposals = Proposals(def, state);
    CHECK(std::find(proposals.begin(), proposals.end(), Trouble{TroubleKind::Leave, wynn, -1}) != proposals.end());

    // 4. Leaving: gone from every rule, coins and all; the town goes on and still adds up.
    REQUIRE(Happen(def, state, {TroubleKind::Leave, wynn, -1}));
    CHECK(state.residents[static_cast<std::size_t>(wynn)].gone == Gone::Left);
    CHECK(Coins(state) - state.minted == before);
    CHECK(Legal(def, state, {wynn, 0}) == Refusal::Gone);
    CHECK(LegalIntents(def, state, wynn).empty());
    CHECK(TalkTo(def, state, wynn).empty());
    CHECK_FALSE(Possible(def, state, {TroubleKind::Death, wynn, -1}));
    CHECK_FALSE(Happen(def, state, {TroubleKind::Feud, edda, wynn}));
    const int tick = state.residents[static_cast<std::size_t>(wynn)].until;
    Run(def, state, 3 * ticksPerDay);
    CHECK(state.residents[static_cast<std::size_t>(wynn)].until == tick); // nothing moves them any more
    for (const Trouble &proposal : Proposals(def, state))
        CHECK((proposal.who != wynn && proposal.other != wynn));

    // 5. It all goes through a save.
    TownState loaded;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(LoadTownState(def, nlohmann::json::parse(SaveTown(def, state).dump()), loaded, diagnostics));
    CHECK(Fingerprint(loaded) == Fingerprint(state));
    // Every kind has its sentence.
    for (int kind = 0; kind < 5; ++kind)
        CHECK_FALSE(Sentence(def, Trouble{static_cast<TroubleKind>(kind), tobin, edda}).empty());
}

TEST_CASE("A plan's mistakes are found before any town is run (T93)", "[story]")
{
    const TownDef def = Settlement();
    std::vector<core::Diagnostic> diagnostics;
    PlanDef plan;
    REQUIRE(LoadPlan(assetDir / "town/settlement.plan.json", def, plan, diagnostics));
    CHECK(plan.beats.size() == 3);
    CHECK(plan.never.size() == 4);

    CHECK(Code(def, R"("beats": [{"id": "a", "from": 1, "to": 2, "outcomes": [{"kind": "plague"}]}])") == "plan.kind");
    CHECK(Code(def, R"("beats": [{"id": "a", "from": 1, "to": 2, "outcomes": [{"kind": "theft", "who": "nobody"}]}])") == "plan.resident");
    CHECK(Code(def, R"("beats": [{"id": "a", "from": 1, "to": 2, "outcomes": [{"kind": "ill", "other": "pip"}]}])") == "plan.resident");
    CHECK(Code(def, R"("beats": [{"id": "a", "from": 5, "to": 2, "outcomes": [{"kind": "ill"}]}])") == "plan.number");
    CHECK(Code(def, R"("beats": [{"id": "a", "from": 6, "to": 8, "outcomes": [{"kind": "ill"}]}, {"id": "b", "from": 2, "to": 4, "outcomes": [{"kind": "ill"}]}])") == "plan.order");
    CHECK(Code(def, R"("beats": [{"id": "a", "from": 1, "to": 2, "outcomes": [{"kind": "ill"}]}, {"id": "a", "from": 2, "to": 4, "outcomes": [{"kind": "ill"}]}])") == "plan.id");
    // Every outcome ruled out, by `never` or by the beat itself.
    CHECK(Code(def, R"("never": [{"kind": "theft"}], "beats": [{"id": "a", "from": 1, "to": 2, "outcomes": [{"kind": "theft", "who": "pip"}]}])") == "plan.dead");
    CHECK(Code(def, R"("beats": [{"id": "a", "from": 1, "to": 2, "outcomes": []}])") == "plan.dead");
    // Two beats that each cost the town someone, but it may lose only one.
    CHECK(Code(def, R"("beats": [{"id": "a", "from": 1, "to": 2, "outcomes": [{"kind": "leave"}]}, {"id": "b", "from": 3, "to": 4, "outcomes": [{"kind": "death"}]}])") == "plan.gone");
    CHECK(Code(def, R"("format": "x")") == "plan.format");

    // A row has no first and second.
    const Pattern row{TroubleKind::Feud, def.Resident("tobin"), def.Resident("edda")};
    CHECK(row.Matches({TroubleKind::Feud, def.Resident("edda"), def.Resident("tobin")}));
    CHECK_FALSE(row.Matches({TroubleKind::Theft, def.Resident("edda"), def.Resident("tobin")}));
}

TEST_CASE("The filter: forbidden, too early, over budget and unallowed deaths are refused (T93)", "[story]")
{
    const TownDef def = Settlement();
    const int tobin = def.Resident("tobin"), hale = def.Resident("hale"), pip = def.Resident("pip"), bren = def.Resident("bren");
    const PlanDef plan = Plan(def, R"("pacing": {"maxIll": 1, "maxGone": 1},
        "never": [{"kind": "theft", "who": "pip"}], "allow": [{"kind": "leave", "who": "hale", "from": 3, "to": 4}],
        "beats": [{"id": "a", "from": 3, "to": 5, "outcomes": [{"kind": "feud", "who": "tobin"}, {"kind": "death", "who": "bren"}], "forbid": [{"kind": "ill", "who": "tobin"}]}])");
    TownState state = NewTown(def, 2);
    StoryState story = NewStory(plan);
    Run(def, state, ticksPerDay); // day 2
    CHECK_FALSE(Permitted(def, plan, state, story, {TroubleKind::Theft, pip, tobin}));  // never
    CHECK(Permitted(def, plan, state, story, {TroubleKind::Theft, hale, tobin}));
    CHECK_FALSE(Permitted(def, plan, state, story, {TroubleKind::Ill, tobin, -1}));     // the beat forbids it
    CHECK_FALSE(Permitted(def, plan, state, story, {TroubleKind::Feud, hale, tobin}));  // the beat's outcome, before its window
    CHECK(Permitted(def, plan, state, story, {TroubleKind::Feud, hale, pip}));          // another row is nobody's business
    CHECK_FALSE(Permitted(def, plan, state, story, {TroubleKind::Leave, hale, -1}));    // allowed, but not yet
    CHECK_FALSE(Permitted(def, plan, state, story, {TroubleKind::Death, bren, -1}));    // the beat's, too early
    CHECK_FALSE(Permitted(def, plan, state, story, {TroubleKind::Death, hale, -1}));    // nobody said Hale may die
    Run(def, state, ticksPerDay); // day 3
    CHECK(Permitted(def, plan, state, story, {TroubleKind::Feud, hale, tobin}));
    CHECK(Permitted(def, plan, state, story, {TroubleKind::Leave, hale, -1}));
    CHECK(Permitted(def, plan, state, story, {TroubleKind::Death, bren, -1}));
    // Budgets: one ill at a time, one gone in all.
    REQUIRE(Happen(def, state, {TroubleKind::Ill, hale, -1}));
    CHECK_FALSE(Permitted(def, plan, state, story, {TroubleKind::Ill, pip, -1}));
    REQUIRE(Happen(def, state, {TroubleKind::Leave, hale, -1}));
    CHECK(Permitted(def, plan, state, story, {TroubleKind::Ill, pip, -1}));
    CHECK_FALSE(Permitted(def, plan, state, story, {TroubleKind::Death, bren, -1}));
}

TEST_CASE("A beat the town does not reach is forced on its last day; an impossible one is missed (T93)", "[story]")
{
    const TownDef def = Settlement();
    // 1. Nothing leads to Edda and Garrick rowing by day 2: forced, with a warning.
    const PlanDef forced = Plan(def, R"("beats": [{"id": "row", "from": 2, "to": 2, "outcomes": [{"kind": "feud", "who": "edda", "other": "garrick"}]}])");
    TownState state = NewTown(def, 1);
    StoryState story = NewStory(forced);
    RunStory(def, forced, state, story, 3 * ticksPerDay);
    CHECK(story.beats[0].how == BeatHow::Forced);
    CHECK(story.beats[0].tick / ticksPerDay + 1 == 2);
    REQUIRE(story.warnings.size() == 1);
    CHECK(story.warnings[0] == "day 2: beat \"row\" had to be forced: Edda and Garrick had a row in front of everyone");
    CHECK(story.Next() == -1);

    // 2. A beat that needs someone who has gone cannot be forced: missed, and said so.
    const PlanDef missed = Plan(def, R"("allow": [{"kind": "leave", "who": "hale"}],
        "beats": [{"id": "late", "from": 3, "to": 3, "outcomes": [{"kind": "ill", "who": "hale"}]}])");
    state = NewTown(def, 1);
    story = NewStory(missed);
    Run(def, state, ticksPerDay);
    REQUIRE(Happen(def, state, {TroubleKind::Leave, def.Resident("hale"), -1}));
    RunStory(def, missed, state, story, 3 * ticksPerDay);
    CHECK(story.beats[0].how == BeatHow::Missed);
    CHECK(story.warnings.back().find("was missed") != std::string::npos);

    // 3. The story so far goes through a save; another plan's story is refused.
    StoryState loaded;
    REQUIRE(LoadStory(missed, nlohmann::json::parse(SaveStory(story).dump()), loaded));
    CHECK(loaded.beats[0].how == BeatHow::Missed);
    CHECK(loaded.troubles == story.troubles);
    const TownDef same = Settlement();
    std::vector<core::Diagnostic> diagnostics;
    PlanDef settlement;
    REQUIRE(LoadPlan(assetDir / "town/settlement.plan.json", same, settlement, diagnostics));
    CHECK_FALSE(LoadStory(settlement, SaveStory(story), loaded));
}

TEST_CASE("The same seed tells the same story; rehearsing the future changes nothing (T93)", "[story]")
{
    const TownDef def = Settlement();
    std::vector<core::Diagnostic> diagnostics;
    PlanDef plan;
    REQUIRE(LoadPlan(assetDir / "town/settlement.plan.json", def, plan, diagnostics));
    TownState a = NewTown(def, 11), b = NewTown(def, 11);
    StoryState storyA = NewStory(plan), storyB = NewStory(plan);
    RunStory(def, plan, a, storyA, 13 * ticksPerDay);
    // The second one is saved and loaded half way.
    RunStory(def, plan, b, storyB, 6 * ticksPerDay);
    TownState loaded;
    REQUIRE(LoadTownState(def, nlohmann::json::parse(SaveTown(def, b).dump()), loaded, diagnostics));
    StoryState loadedStory;
    REQUIRE(LoadStory(plan, nlohmann::json::parse(SaveStory(storyB).dump()), loadedStory));
    RunStory(def, plan, loaded, loadedStory, 7 * ticksPerDay);
    CHECK(Fingerprint(a) == Fingerprint(loaded));
    for (std::size_t i = 0; i < plan.beats.size(); ++i)
    {
        CHECK(storyA.beats[i].outcome == loadedStory.beats[i].outcome);
        CHECK(storyA.beats[i].tick == loadedStory.beats[i].tick);
    }
    CHECK(storyA.Next() == -1);
}

TEST_CASE("A thousand playthroughs: the plot always finishes, nothing forbidden happens, the roads differ (T93)", "[story]")
{
    const TownDef def = Settlement();
    std::vector<core::Diagnostic> diagnostics;
    PlanDef plan;
    REQUIRE(LoadPlan(assetDir / "town/settlement.plan.json", def, plan, diagnostics));
    const SoakReport report = Soak(def, plan, 1, 1000, 13);
    INFO(Text(plan, report));
    CHECK(report.Holds());
    CHECK(report.forbidden == 0);
    CHECK(report.complete == 1000);
    for (const SoakBeat &beat : report.beats)
    {
        CHECK(beat.missed == 0);
        CHECK(beat.outcomes.size() >= 3); // each beat is reached at least three different ways
        CHECK(beat.natural > beat.forced); // mostly the town gets there by itself
    }
    CHECK(report.roads.size() >= 20);
    // Pacing: with a day of peace after each trouble, 13 days hold at most 7 of them.
    CHECK(report.troubles <= 7 * 1000);
    // Looking ahead earns its keep: without it, more beats have to be forced.
    PlanDef blind = plan;
    blind.lookAhead = 0;
    const SoakReport without = Soak(def, blind, 1, 300, 13), with = Soak(def, plan, 1, 300, 13);
    int forcedWithout = 0, forcedWith = 0;
    for (std::size_t b = 0; b < plan.beats.size(); ++b)
        forcedWithout += without.beats[b].forced, forcedWith += with.beats[b].forced;
    CHECK(forcedWith < forcedWithout);
    CHECK(without.Holds());
}
