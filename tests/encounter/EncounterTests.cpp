#include "RpgTestContent.hpp"

#include <EncounterCoordinator.hpp>
#include <Inventory.hpp>
#include <SkillTree.hpp>

#include <glm/gtc/constants.hpp>

using namespace umbra;
using encounter::State;
using rpg::Rejection;

namespace
{
    const rpg::Content &C() { return rpgtest::Reference(); }
    constexpr float frame = 1.0F / 60.0F;
}

TEST_CASE("One fight at a time; decisions from a finished fight are stale")
{
    rpg::Character hero = rpgtest::Make("hero", "hero");
    rpg::Character wolf = rpgtest::Make("wolf", "wolf_1");
    encounter::EncounterCoordinator coordinator(C());
    REQUIRE(coordinator.GetState() == State::Exploration);
    REQUIRE(coordinator.Submit(1, {1, 0, "strike", "", 1}) == Rejection::Stale);

    const std::uint64_t first = coordinator.BeginRealtime({{&hero, 0}, {&wolf, 1}});
    REQUIRE(first != 0);
    REQUIRE(coordinator.BeginTurn({{&hero, 0}, {&wolf, 1}}) == 0); // already fighting
    REQUIRE(coordinator.Turn() == nullptr);                        // only one controller exists
    REQUIRE(coordinator.Realtime() != nullptr);

    REQUIRE(coordinator.Finish() == -1); // fled undecided
    REQUIRE(coordinator.Realtime() == nullptr);

    const std::uint64_t second = coordinator.BeginTurn({{&hero, 0}, {&wolf, 1}});
    REQUIRE(second != first);
    // A real-time decision still in flight from the first fight is rejected, not applied (T47).
    REQUIRE(coordinator.Submit(first, {2, 1, "bite", "", 0}) == Rejection::Stale);
    REQUIRE(hero.health == 110);
    REQUIRE(coordinator.Submit(second, {3, 1, "bite", "", 0}) == Rejection::None); // wolf is faster
}

TEST_CASE("Combined fixture: a real-time fight, then a turn fight, with shared progression")
{
    rpg::Character hero = rpgtest::Make("hero", "hero");
    rpg::Character wolf = rpgtest::Make("wolf", "wolf_1");
    rpg::Character bandit = rpgtest::Make("bandit", "bandit_1");
    encounter::EncounterCoordinator coordinator(C());
    std::uint64_t nextId = 1;

    // 1. Real-time: hero against the wolf, both on the conventional AI.
    const std::uint64_t fight1 = coordinator.BeginRealtime({{&hero, 0}, {&wolf, 1}});
    realtime::RealtimeEncounter &rt = *coordinator.Realtime();
    rt.SetPlacement(1, {0.0F, -4.0F}, glm::pi<float>());
    for (int f = 0; f < 60 * 60 && coordinator.GetState() == State::Realtime; ++f)
    {
        for (std::size_t i = 0; i < 2; ++i)
        {
            realtime::Decision d = realtime::ChooseRealtimeAction(C(), rt, i);
            rt.SetPlacement(i, rt.GetPosition(i) + d.move * 3.0F * frame, d.yaw);
            if (d.act)
            {
                d.act->id = nextId++;
                (void)coordinator.Submit(fight1, *d.act);
            }
        }
        coordinator.Update(frame);
    }
    REQUIRE(coordinator.GetState() == State::Resolution);
    REQUIRE(coordinator.Submit(fight1, {nextId++, 0, "strike", "", 1}) == Rejection::EncounterOver);
    hero.statuses.push_back({"guarded", 0, 3.0F}); // pretend a status was still running
    REQUIRE(coordinator.Finish() == 0);
    REQUIRE(hero.statuses.empty()); // statuses end with the encounter
    const int healthAfterWolf = hero.health;
    REQUIRE(healthAfterWolf < 110); // damage carries over: no free heal between fights

    // 2. Between fights: spend a skill point; it is usable in the next fight.
    REQUIRE(rpg::Unlock(C(), hero, "heavy_blow") == rpg::UnlockResult::Unlocked);

    // 3. Turn-based: hero against the bandit.
    const std::uint64_t fight2 = coordinator.BeginTurn({{&hero, 0}, {&bandit, 1}});
    turn::TurnEncounter &tb = *coordinator.Turn();
    REQUIRE(tb.Current() == 0u); // hero speed 10 > bandit 8
    REQUIRE(coordinator.Submit(fight2, {nextId++, 0, "heavy_blow", "", 1}) == Rejection::None);
    REQUIRE(bandit.health == 70 - rpg::DamageAmount(18, 14, 4));
    while (coordinator.GetState() == State::TurnBased)
    {
        rpg::ActionIntent intent = turn::ChooseTurnAction(C(), tb);
        intent.id = nextId++;
        REQUIRE(coordinator.Submit(fight2, intent) == Rejection::None);
    }
    REQUIRE(coordinator.Finish() == 0);
    REQUIRE(hero.health > 0);
    REQUIRE(hero.health <= healthAfterWolf + 30 * 3); // only potions healed in between
    REQUIRE(coordinator.GetState() == State::Exploration);
}

TEST_CASE("Ids from separate controllers never collide in the log of the next fight")
{
    rpg::Character hero = rpgtest::Make("hero", "hero");
    rpg::Character wolf = rpgtest::Make("wolf", "wolf_1");
    encounter::EncounterCoordinator coordinator(C());
    const std::uint64_t a = coordinator.BeginTurn({{&hero, 0}, {&wolf, 1}});
    REQUIRE(coordinator.Submit(a, {1, 1, "", "", 1}) == Rejection::None); // wolf waits, id 1
    coordinator.Finish();
    // A new fight has a new log: id 1 is a fresh decision there, and it is a different fight.
    const std::uint64_t b = coordinator.BeginTurn({{&hero, 0}, {&wolf, 1}});
    REQUIRE(coordinator.Submit(b, {1, 1, "", "", 1}) == Rejection::None);
}
