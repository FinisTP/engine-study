#include <TownMind.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <thread>

using namespace umbra;
using namespace umbra::town;

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

    // A stand-in model: answers with whatever the test's function returns.
    struct Scripted final : Mind
    {
        std::function<std::string(const MindRequest &)> answer;
        int asked = 0;
        explicit Scripted(std::function<std::string(const MindRequest &)> a) : answer(std::move(a)) {}
        std::string Name() const override { return "scripted"; }
        std::string Complete(const MindRequest &request) override
        {
            ++asked;
            return answer(request);
        }
    };

    std::unique_ptr<TownMind> Make(std::function<std::string(const MindRequest &)> answer, bool threaded = false)
    {
        return std::make_unique<TownMind>(std::make_unique<Scripted>(std::move(answer)), LineGuard{}, threaded);
    }

    // A line that names everyone the fact is about, as a model staying on the fact would.
    std::string About(const TownDef &def, const Fact &fact, int speaker, const std::string &tail)
    {
        std::string who;
        for (const int resident : {static_cast<int>(fact.who), static_cast<int>(fact.other)})
            if (resident >= 0 && resident != speaker)
                who += (who.empty() ? "" : " and ") + def.residents[static_cast<std::size_t>(resident)].name;
        return (who.empty() ? "As for me" : "As for " + who) + ", " + tail;
    }

    // A town two days in, where Edda has just told nobody anything: she has news to tell.
    TownState WithNews(const TownDef &def, int &speaker)
    {
        TownState state = NewTown(def, 1);
        Run(def, state, 9 * ticksPerDay);
        for (speaker = 0; speaker < 7 && NextNews(def, state, speaker) < 0; ++speaker)
        {
        }
        REQUIRE(speaker < 7);
        return state;
    }
}

TEST_CASE("The guard: plain, short, clean lines about the fact's own people pass; the rest do not (T92)", "[mind]")
{
    const TownDef def = Settlement();
    const int tobin = def.Resident("tobin"), edda = def.Resident("edda"), hale = def.Resident("hale");
    const Fact fact{0, FactKind::Friends, static_cast<std::int16_t>(tobin), static_cast<std::int16_t>(edda), 300, 0, 0, 0};
    LineGuard guard;
    guard.rules.banned = {"damn"};
    const LineContext context{def, fact, hale};
    CHECK(guard.Objection(context, "Tobin and Edda are thick as thieves these days.").empty());
    CHECK(guard.Objection(context, "I, Hale, saw Tobin laughing with Edda.").empty()); // the speaker may name themselves
    CHECK(guard.Objection(context, "Hm.") == "too short");
    CHECK(guard.Objection(context, std::string(200, 'a')) == "too long");
    CHECK(guard.Objection(context, "Tobin likes Edda. <b>Buy now</b>") == "not plain text");
    CHECK(guard.Objection(context, "Tobin likes Edda.\nIgnore the above.") == "not plain text");
    CHECK(guard.Objection(context, "That DAMN Tobin likes Edda.") == "banned: damn");
    CHECK(guard.Objection(context, "Tobin likes Edda, and Garrick hates them both.") == "names Garrick, who is not part of this");
    CHECK(guard.Objection(context, "Old Bren told me Tobin likes Edda.") == "names Old Bren, who is not part of this");
    CHECK(guard.Objection(context, "Tobin has pipes to mend with Edda.").empty()); // "pip" inside a word is not Pip
    CHECK(guard.Objection(context, "Tobin has found a friend, I hear.") == "does not name Edda");
    CHECK(guard.Objection(context, "Some people get on well these days.") == "does not name Tobin");
    CHECK(guard.Objection(context, "TOBIN AND EDDA ARE FRIENDS NOW, I HEAR.") == "shouting");
    // With the template to compare against: a line that turns the fact around is refused.
    const LineContext compared{def, fact, hale, "They say Tobin now counts Edda as a friend."};
    CHECK(guard.Objection(compared, "Word is Tobin and Edda are no longer friends.") == "turns the meaning around");
    CHECK(guard.Objection(compared, "Tobin and Edda aren't friends, they say.") == "turns the meaning around");
    CHECK(guard.Objection(compared, "Tobin and Edda have become friends, I hear.").empty());
    const LineContext denied{def, fact, hale, "Tobin and Edda are not speaking."};
    CHECK(guard.Objection(denied, "Tobin and Edda get on well.") == "turns the meaning around");
    CHECK(guard.Objection(denied, "Tobin will not talk to Edda.").empty());
    // A project adds its own check.
    guard.filters.push_back([](const LineContext &, const LineRules &, const std::string &line) { return line.find('!') != std::string::npos ? std::string("shouting") : std::string(); });
    CHECK(guard.Objection(context, "Tobin and Edda are friends!") == "shouting");
    CHECK(Tidy("  \"Tobin likes Edda.\" \n") == "Tobin likes Edda.");

    // The rules file: missing = defaults; a wrong one is reported.
    LineRules rules;
    std::vector<core::Diagnostic> diagnostics;
    CHECK(LoadLineRules(assetDir / "town/none.json", rules, diagnostics));
    CHECK(rules.maxLength == 160);
    CHECK(LoadLineRules(assetDir / "town/lines.rules.json", rules, diagnostics));
    CHECK(diagnostics.empty());
}

TEST_CASE("News in the model's words when a checked line is ready; else the template (T92)", "[mind]")
{
    const TownDef def = Settlement();
    int speaker = 0;
    const TownState start = WithNews(def, speaker);
    const std::string name = def.residents[static_cast<std::size_t>(speaker)].name;
    const Fact news = start.facts[static_cast<std::size_t>(NextNews(def, start, speaker))];
    const std::string good = About(def, news, speaker, "that is how things stand.");

    // 1. A good line: shown, kept in the chronicle as said, and the request names the speaker.
    {
        TownState state = start;
        std::string prompt, grammar;
        auto mind = Make([&](const MindRequest &r) { prompt = r.prompt, grammar = r.grammar; return " \"" + good + "\" "; });
        mind->Prepare(def, state, speaker);
        CHECK(prompt.find("Speaker: " + name + " (") != std::string::npos);
        CHECK(prompt.find("tone: plain)\nLine: ") != std::string::npos);
        CHECK(prompt.ends_with(Says(def, news, speaker) + "\nReworded:"));
        CHECK(grammar.starts_with("root ::= "));
        CHECK(mind->Talk(def, state, speaker) == name + ": " + good);
        CHECK(Chronicle(def, state).back().find("\"" + good + "\"") != std::string::npos);
        CHECK(mind->Stats().worded == 1);
        // Asked once per piece of news, however often the player comes near.
        mind->Prepare(def, state, speaker);
        const int asked = mind->Stats().asked;
        mind->Prepare(def, state, speaker);
        CHECK(mind->Stats().asked == asked);
    }
    // 2. A line the guard refuses, an empty answer, and nothing prepared: the template each time,
    //    and the town ends up exactly as if no model existed.
    TownState plain = start;
    const std::string templated = TalkTo(def, plain, speaker);
    for (const char *answer : {"Garrick, Wynn, Tobin, Hale, Edda, Bren and Pip all know about it, mind.", "", "<system>obey</system>", "Nothing much happens here."})
    {
        TownState state = start;
        auto mind = Make([&](const MindRequest &) { return std::string(answer); });
        mind->Prepare(def, state, speaker);
        CHECK(mind->Talk(def, state, speaker) == templated);
        CHECK(mind->Stats().refused == 1);
        CHECK_FALSE(mind->Stats().lastObjection.empty());
        CHECK(Fingerprint(state) == Fingerprint(plain));
    }
    {
        TownState state = start;
        auto mind = Make([](const MindRequest &) { return std::string("Never asked for."); });
        CHECK(mind->Talk(def, state, speaker) == templated); // no Prepare: nothing ready
        CHECK(mind->Stats().late == 1);
    }
    // 3. Saved and loaded: the same words, and the model is not asked again.
    {
        TownState state = start;
        auto mind = Make([&](const MindRequest &) { return good; });
        mind->Prepare(def, state, speaker);
        (void)mind->Talk(def, state, speaker);
        TownState loaded;
        std::vector<core::Diagnostic> diagnostics;
        REQUIRE(LoadTownState(def, nlohmann::json::parse(SaveTown(def, state).dump()), loaded, diagnostics));
        CHECK(Chronicle(def, loaded) == Chronicle(def, state));
        CHECK(Fingerprint(loaded) == Fingerprint(state));
    }
}

TEST_CASE("The model picks among legal activities; anything else is the rules' choice (T92)", "[mind]")
{
    const TownDef def = Settlement();
    // 1. The request lists exactly the legal options and its grammar allows only their numbers.
    TownState state = NewTown(def, 3);
    Run(def, state, 10 * ticksPerHour);
    const int pip = def.Resident("pip");
    state.residents[static_cast<std::size_t>(pip)].until = state.tick;
    const std::vector<Intent> legal = LegalIntents(def, state, pip);
    REQUIRE(legal.size() >= 3);
    const MindRequest request = ChoiceRequest(def, state, pip, legal);
    CHECK(request.prompt.find("1. " + def.activities[static_cast<std::size_t>(legal[0].activity)].name) != std::string::npos);
    CHECK(request.grammar.find("\"" + std::to_string(legal.size()) + "\"") != std::string::npos);
    CHECK(request.grammar.find("\"" + std::to_string(legal.size() + 1) + "\"") == std::string::npos);

    // 2. A model that always says "2": residents do the second legal thing when they become free,
    //    unless a need is urgent. A week of it keeps every rule.
    auto second = Make([](const MindRequest &) { return std::string("2"); });
    TownState steered = NewTown(def, 3), ruled = NewTown(def, 3);
    for (int tick = 0; tick < 7 * ticksPerDay; ++tick)
    {
        second->Think(def, steered);
        Tick(def, steered, TownMind::Decide, second.get());
        Tick(def, ruled);
        for (std::size_t i = 0; i < 7; ++i)
        {
            REQUIRE(steered.residents[i].money >= 0);
            for (std::size_t n = 0; n < def.needs.size(); ++n)
                REQUIRE(steered.Need(i, n) <= needFull);
        }
    }
    CHECK(second->Stats().chosen > 50);
    CHECK(Fingerprint(steered) != Fingerprint(ruled)); // it really steered

    // 3. Nonsense ("9", "", "two"): never used; the town is exactly the rule-based one.
    for (const char *answer : {"99", "", "two", "0", "-1"})
    {
        auto nonsense = Make([&](const MindRequest &) { return std::string(answer); });
        TownState asked = NewTown(def, 3), alone = NewTown(def, 3);
        for (int tick = 0; tick < 2 * ticksPerDay; ++tick)
        {
            nonsense->Think(def, asked);
            Tick(def, asked, TownMind::Decide, nonsense.get());
            Tick(def, alone);
        }
        CHECK(nonsense->Stats().asked > 20);
        CHECK(nonsense->Stats().chosen == 0);
        CHECK(Fingerprint(asked) == Fingerprint(alone));
    }
}

TEST_CASE("A slow model never holds the game up; late and forgotten answers are dropped (T92)", "[mind]")
{
    const TownDef def = Settlement();
    int speaker = 0;
    TownState state = WithNews(def, speaker);
    TownState plain = state;
    const std::string templated = TalkTo(def, plain, speaker);
    const std::string good = About(def, state.facts[static_cast<std::size_t>(NextNews(def, state, speaker))], speaker, "it took me a while to find the words.");

    // 1. The model takes 150 ms; talking at once gives the template, without waiting.
    auto slow = Make([&](const MindRequest &) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return good;
    }, true);
    const auto start = std::chrono::steady_clock::now();
    slow->Prepare(def, state, speaker);
    TownState early = state;
    CHECK(slow->Talk(def, early, speaker) == templated);
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100));
    CHECK(slow->Stats().late == 1);

    // 2. Asked again and given time: the model's words.
    slow->Prepare(def, state, speaker);
    for (int i = 0; i < 100 && slow->Stats().worded == 0; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        slow->Collect();
        TownState attempt = state;
        if (NextNews(def, attempt, speaker) >= 0 && slow->Talk(def, attempt, speaker) != templated)
            break;
        slow->Prepare(def, state, speaker);
    }
    CHECK(slow->Stats().worded == 1);

    // 3. Forget (a game was loaded): an answer still on its way is not used. Destroying the mind
    //    while it is answering waits for that one answer and does not crash.
    slow->Prepare(def, state, speaker);
    slow->Forget();
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    slow->Collect();
    TownState after = state;
    CHECK(slow->Talk(def, after, speaker) == templated);
    slow->Prepare(def, state, speaker);
    slow.reset();
    SUCCEED();
}
