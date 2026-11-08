#include "RpgTestContent.hpp"

#include <Inventory.hpp>
#include <Rules.hpp>
#include <SkillTree.hpp>

using namespace umbra::rpg;

namespace
{
    const Content &C() { return rpgtest::Reference(); }

    // hero (team 0) against two wolves (team 1).
    struct Fight
    {
        Character hero = rpgtest::Make("hero", "hero");
        Character wolf1 = rpgtest::Make("wolf", "wolf_1");
        Character wolf2 = rpgtest::Make("wolf", "wolf_2");
        Roster roster{{&hero, 0}, {&wolf1, 1}, {&wolf2, 1}};
        Events events;

        // Check + pay + apply, like a turn controller does.
        Rejection Act(ActionIntent intent)
        {
            const Rejection r = CheckAction(C(), roster, intent, Mode::Turn);
            if (r == Rejection::None)
            {
                PayCost(C(), roster, intent, events);
                ApplyEffects(C(), roster, intent, TurnTargets(C(), intent), Mode::Turn, events);
            }
            return r;
        }
        int Count(EventKind kind) const
        {
            return static_cast<int>(std::count_if(events.begin(), events.end(), [&](const Event &e) { return e.kind == kind; }));
        }
    };

    ActionIntent Skill(std::size_t actor, const std::string &skill, std::size_t target)
    {
        return {0, actor, skill, "", target};
    }
}

TEST_CASE("Damage is power + attack - defense, at least 1")
{
    REQUIRE(DamageAmount(8, 14, 2) == 20);
    REQUIRE(DamageAmount(1, 1, 50) == 1);
    Fight f;
    REQUIRE(f.Act(Skill(0, "strike", 1)) == Rejection::None);
    REQUIRE(f.wolf1.health == 25); // 45 - 20
    REQUIRE(f.events.back().kind == EventKind::Damage);
}

TEST_CASE("Illegal actions are rejected and change nothing (T46)")
{
    Fight f;
    f.hero.energy = 3;
    const Character before = f.hero;
    REQUIRE(f.Act(Skill(0, "heavy_blow", 1)) == Rejection::SkillLocked);
    REQUIRE(Unlock(C(), f.hero, "heavy_blow") == UnlockResult::Unlocked);
    REQUIRE(f.Act(Skill(0, "heavy_blow", 1)) == Rejection::NotEnoughEnergy);
    REQUIRE(f.Act(Skill(0, "strike", 0)) == Rejection::BadTarget);  // own team
    REQUIRE(f.Act(Skill(0, "strike", 9)) == Rejection::BadTarget);  // no such fighter
    REQUIRE(f.Act(Skill(0, "fireball", 1)) == Rejection::UnknownSkill);
    REQUIRE(f.Act({0, 0, "", "iron_sword", 0}) == Rejection::NoSuchItem);
    REQUIRE(f.Act({0, 0, "", "health_potion", 0}) == Rejection::None); // legal: drinks one
    REQUIRE(f.Act({0, 9, "strike", "", 1}) == Rejection::UnknownActor);
    REQUIRE(f.Act({0, 0, "strike", "health_potion", 1}) == Rejection::UnknownSkill); // both set

    f.hero.skills.insert("weaken");
    f.hero.energy = 30;
    REQUIRE(CheckAction(C(), f.roster, Skill(0, "weaken", 1), Mode::Realtime) == Rejection::WrongMode);
    REQUIRE(f.wolf1.health == 45);
    REQUIRE(f.wolf2.health == 45);
    REQUIRE(before.skillPoints == 3);
}

TEST_CASE("A target that died after being chosen is rejected (T07)")
{
    Fight f;
    f.wolf1.health = 0;
    REQUIRE(f.Act(Skill(0, "strike", 1)) == Rejection::TargetDead);
    f.hero.health = 0;
    REQUIRE(f.Act(Skill(0, "strike", 2)) == Rejection::ActorDead);
    REQUIRE(f.events.empty());
}

TEST_CASE("Items: a potion heals up to the maximum and is consumed once")
{
    Fight f;
    f.hero.health = 100; // max 110
    REQUIRE(f.Act({0, 0, "", "health_potion", 0}) == Rejection::None);
    REQUIRE(f.hero.health == 110); // healed 10, not 30
    REQUIRE(CountItem(f.hero, "health_potion") == 2);
    REQUIRE(f.Count(EventKind::ItemUsed) == 1);
    REQUIRE(f.events.back().amount == 10);
}

TEST_CASE("Statuses refresh instead of stacking and tick on their own clock")
{
    Fight f;
    f.hero.skills.insert("venom_strike");
    REQUIRE(f.Act(Skill(0, "venom_strike", 1)) == Rejection::None);
    REQUIRE(f.Act(Skill(0, "venom_strike", 1)) == Rejection::None);
    REQUIRE(f.wolf1.statuses.size() == 1);
    REQUIRE(f.wolf1.statuses[0].turnsLeft == 3);
    REQUIRE(f.hero.energy == 30 - 16);

    const int health = f.wolf1.health;
    for (int turn = 0; turn < 4; ++turn)
        TickStatusesTurn(C(), f.roster, 1, f.events);
    REQUIRE(f.wolf1.health == health - 9); // 3 turns x 3, then gone
    REQUIRE(f.wolf1.statuses.empty());
    REQUIRE(f.Count(EventKind::StatusExpired) == 1);

    // Real-time: 6 seconds of poison ticks once per second.
    f.wolf2.statuses.push_back({"poison", 0, 6.0F, 1.0F});
    for (int i = 0; i < 16; ++i)
        TickStatusesSeconds(C(), f.roster, 0.5F, f.events);
    REQUIRE(f.wolf2.health == 45 - 18);
    REQUIRE(f.wolf2.statuses.empty());
}

TEST_CASE("Several deaths in one action resolve in target order, each reported once (T10)")
{
    Fight f;
    f.wolf1.health = 5;
    f.wolf2.health = 5;
    f.wolf2.statuses.push_back({"poison", 3});
    ApplyEffects(C(), f.roster, Skill(0, "strike", 0), {2, 1}, Mode::Realtime, f.events);
    REQUIRE(f.Count(EventKind::Death) == 2);
    REQUIRE(f.events[2].kind == EventKind::Death);
    REQUIRE(f.events[2].target == 2); // wolf_2 first, as given
    REQUIRE(f.events[3].target == 1);
    REQUIRE(f.wolf2.statuses.empty()); // the dead keep no statuses

    ApplyEffects(C(), f.roster, Skill(0, "strike", 0), {1}, Mode::Realtime, f.events);
    REQUIRE(f.Count(EventKind::Death) == 2); // hitting a corpse does nothing
    REQUIRE(WinningTeam(f.roster) == 0);
}

TEST_CASE("Repeated decision ids are recognised")
{
    IntentLog log;
    REQUIRE(log.First(7));
    REQUIRE_FALSE(log.First(7));
    log.Clear();
    REQUIRE(log.First(7));
}

TEST_CASE("Ending an encounter clears statuses")
{
    Character hero = rpgtest::Make("hero", "hero");
    hero.statuses.push_back({"guarded", 2});
    EndEncounter(hero);
    REQUIRE(hero.statuses.empty());
}
