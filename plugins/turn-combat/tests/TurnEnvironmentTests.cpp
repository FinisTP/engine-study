#include "RpgTestContent.hpp"

#include <TurnEnvironment.hpp>

using namespace umbra;

namespace
{
    const rpg::Content &C() { return rpgtest::Reference(); }
    const std::vector<turn::ScenarioSlot> scenario{{"hero", "hero", 0}, {"wolf", "wolf_1", 1}, {"bandit", "bandit_1", 1}};

    // A fixed pseudo-random choice among the legal actions (same seed -> same episode).
    struct Chooser
    {
        std::uint32_t state;
        std::size_t Pick(std::size_t count)
        {
            state = state * 1664525u + 1013904223u;
            return (state >> 8) % count;
        }
    };

    struct Episode
    {
        std::vector<rpg::Character> finalState;
        std::vector<rpg::ActionIntent> actions;
        int winner = -1;
        int steps = 0;
    };

    Episode Play(turn::TurnEnvironment &env, std::uint32_t seed)
    {
        Episode episode;
        Chooser chooser{seed};
        REQUIRE(env.Reset());
        for (int i = 0; i < 1000 && env.Current(); ++i)
        {
            const auto legal = env.LegalActions();
            const rpg::ActionIntent action = legal[chooser.Pick(legal.size())];
            const turn::StepResult result = env.Step(action);
            REQUIRE(result.rejection == rpg::Rejection::None); // every listed action is accepted
            episode.actions.push_back(action);
            episode.winner = result.winner;
        }
        episode.finalState = env.Characters();
        episode.steps = env.Steps();
        return episode;
    }

    bool Same(const rpg::Character &a, const rpg::Character &b)
    {
        return a.id == b.id && a.health == b.health && a.energy == b.energy && a.statuses.size() == b.statuses.size() &&
               a.skills == b.skills && a.skillPoints == b.skillPoints && a.inventory.size() == b.inventory.size();
    }
}

TEST_CASE("Reset gives a fresh scenario every time, whatever happened before")
{
    turn::TurnEnvironment env(C(), scenario);
    REQUIRE(env.Reset());
    const std::vector<rpg::Character> fresh = env.Characters();
    for (std::uint32_t seed = 1; seed <= 50; ++seed)
    {
        Play(env, seed); // play to the end (statuses, potions, deaths...)
        REQUIRE(env.Reset());
        REQUIRE(env.Round() == 1);
        REQUIRE(env.Steps() == 0);
        for (std::size_t i = 0; i < fresh.size(); ++i)
            REQUIRE(Same(env.Characters()[i], fresh[i]));
    }
    REQUIRE_FALSE(turn::TurnEnvironment(C(), {{"dragon", "d", 1}}).Reset());
}

TEST_CASE("Same seed, same episode; every step uses exactly one turn (T60)")
{
    turn::TurnEnvironment env(C(), scenario);
    const Episode a = Play(env, 42);
    const Episode b = Play(env, 42);
    REQUIRE(a.winner == b.winner);
    REQUIRE(a.steps == b.steps);
    REQUIRE(a.steps == static_cast<int>(a.actions.size()));
    for (std::size_t i = 0; i < a.finalState.size(); ++i)
        REQUIRE(Same(a.finalState[i], b.finalState[i]));

    // One step = one decision by the current fighter, then the next fighter in order.
    REQUIRE(env.Reset());
    REQUIRE(env.Current() == 1u); // wolf
    REQUIRE(env.Step(env.LegalActions().back()).rejection == rpg::Rejection::None);
    REQUIRE(env.Current() == 0u); // hero, not the wolf again
    // A rejected step changes nothing and doesn't use the turn.
    const turn::StepResult bad = env.Step({0, 0, "heavy_blow", "", 1});
    REQUIRE(bad.rejection == rpg::Rejection::SkillLocked);
    REQUIRE(env.Current() == 0u);
    REQUIRE(env.Steps() == 1);
}

TEST_CASE("The environment and the game path give identical results for the same decisions (T55)")
{
    turn::TurnEnvironment env(C(), scenario);
    const Episode episode = Play(env, 7);

    // Replay the recorded decisions through a plain TurnEncounter, the way the game drives it.
    rpg::Character hero = rpgtest::Make("hero", "hero");
    rpg::Character wolf = rpgtest::Make("wolf", "wolf_1");
    rpg::Character bandit = rpgtest::Make("bandit", "bandit_1");
    turn::TurnEncounter game(C(), {{&hero, 0}, {&wolf, 1}, {&bandit, 1}});
    std::uint64_t id = 1000;
    for (rpg::ActionIntent action : episode.actions)
    {
        action.id = id++;
        REQUIRE(game.Submit(action) == rpg::Rejection::None);
    }
    REQUIRE(game.IsOver() == (episode.winner != -1));
    REQUIRE(game.Winner() == episode.winner);
    REQUIRE(Same(hero, episode.finalState[0]));
    REQUIRE(Same(wolf, episode.finalState[1]));
    REQUIRE(Same(bandit, episode.finalState[2]));
}

TEST_CASE("Characters keep durable ids, never handles, across resets (T49)")
{
    turn::TurnEnvironment env(C(), scenario);
    Play(env, 3);
    REQUIRE(env.Reset());
    REQUIRE(env.Characters()[1].id == "wolf_1");
    REQUIRE(env.Characters()[1].actor == "wolf");
    REQUIRE(env.Characters()[2].inventory.front().item == "health_potion"); // ids and counts only
}
