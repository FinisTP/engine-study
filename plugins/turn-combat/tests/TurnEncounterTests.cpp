#include "RpgTestContent.hpp"

#include <Inventory.hpp>
#include <SkillTree.hpp>
#include <TurnEncounter.hpp>

using namespace umbra;
using rpg::Rejection;

namespace
{
    const rpg::Content &C() { return rpgtest::Reference(); }

    // hero (speed 10) against a wolf (12) and a bandit (8).
    struct Battle
    {
        rpg::Character hero = rpgtest::Make("hero", "hero");
        rpg::Character wolf = rpgtest::Make("wolf", "wolf_1");
        rpg::Character bandit = rpgtest::Make("bandit", "bandit_1");
        std::uint64_t nextId = 1;

        rpg::Roster Roster() { return {{&hero, 0}, {&wolf, 1}, {&bandit, 1}}; }

        // Everyone uses the conventional AI until the fight ends.
        int PlayOut(turn::TurnEncounter &encounter, int maxActions = 500)
        {
            int actions = 0;
            while (!encounter.IsOver() && actions < maxActions)
            {
                rpg::ActionIntent intent = turn::ChooseTurnAction(C(), encounter);
                intent.id = nextId++;
                REQUIRE(encounter.Submit(intent) == Rejection::None);
                ++actions;
            }
            return actions;
        }
    };

    rpg::ActionIntent Strike(std::uint64_t id, std::size_t actor, std::size_t target) { return {id, actor, "strike", "", target}; }
    rpg::ActionIntent Wait(std::uint64_t id, std::size_t actor) { return {id, actor, "", "", actor}; }
}

TEST_CASE("Each round everyone acts once, fastest first")
{
    Battle b;
    turn::TurnEncounter encounter(C(), b.Roster());
    REQUIRE(encounter.Round() == 1);
    REQUIRE(encounter.Current() == 1u); // wolf, speed 12
    REQUIRE(encounter.Submit(Wait(1, 1)) == Rejection::None);
    REQUIRE(encounter.Current() == 0u); // hero, 10
    REQUIRE(encounter.Submit(Wait(2, 0)) == Rejection::None);
    REQUIRE(encounter.Current() == 2u); // bandit, 8
    REQUIRE(encounter.Submit(Wait(3, 2)) == Rejection::None);
    REQUIRE(encounter.Round() == 2);
    REQUIRE(encounter.Current() == 1u);
}

TEST_CASE("Out-of-turn, repeated and illegal decisions change nothing")
{
    Battle b;
    turn::TurnEncounter encounter(C(), b.Roster());
    REQUIRE(encounter.Submit(Strike(1, 0, 1)) == Rejection::NotYourTurn); // wolf's turn
    REQUIRE(encounter.Submit({2, 1, "strike", "", 0}) == Rejection::SkillLocked); // wolves only bite
    REQUIRE(encounter.Submit({3, 1, "bite", "", 2}) == Rejection::BadTarget);   // own team
    REQUIRE(encounter.Current() == 1u); // still the wolf's turn
    REQUIRE(b.hero.health == 110);

    REQUIRE(encounter.Submit({4, 1, "bite", "", 0}) == Rejection::None);
    REQUIRE(b.hero.health == 110 - rpg::DamageAmount(6, 8, 8)); // bite vs hero defense 8
    REQUIRE(encounter.Submit({4, 0, "strike", "", 1}) == Rejection::DuplicateIntent); // id 4 again
    REQUIRE(b.wolf.health == 45);
    REQUIRE(encounter.Current() == 0u);
}

TEST_CASE("Statuses tick at the start of the affected fighter's turn")
{
    Battle b;
    b.hero.skills.insert("venom_strike");
    turn::TurnEncounter encounter(C(), b.Roster());
    REQUIRE(encounter.Submit(Wait(1, 1)) == Rejection::None);
    REQUIRE(encounter.Submit({2, 0, "venom_strike", "", 1}) == Rejection::None); // wolf poisoned
    const int afterHit = b.wolf.health;
    REQUIRE(encounter.Submit(Wait(3, 2)) == Rejection::None); // round 2 starts with the wolf
    REQUIRE(b.wolf.health == afterHit - 3);                   // poison ticked once
    const auto events = encounter.TakeEvents();
    REQUIRE(std::any_of(events.begin(), events.end(), [](const rpg::Event &e) { return e.kind == rpg::EventKind::StatusTick; }));
}

TEST_CASE("A fighter killed by a status at turn start is skipped; the last one standing wins")
{
    Battle b;
    b.wolf.health = 2;
    b.wolf.statuses.push_back({"poison", 3});
    b.bandit.health = 0;
    turn::TurnEncounter encounter(C(), b.Roster());
    REQUIRE(encounter.IsOver()); // the wolf's first turn tick killed it
    REQUIRE(encounter.Winner() == 0);
    REQUIRE_FALSE(encounter.Current());
    REQUIRE(encounter.Submit(Wait(1, 0)) == Rejection::EncounterOver);
}

TEST_CASE("Legal actions: affordable skills per living enemy, usable items, then wait")
{
    Battle b;
    REQUIRE(rpg::Unlock(C(), b.hero, "guard") == rpg::UnlockResult::Unlocked);
    b.bandit.health = 0;
    turn::TurnEncounter encounter(C(), b.Roster());
    REQUIRE(encounter.Submit(Wait(1, 1)) == Rejection::None);
    const auto legal = encounter.LegalActions(); // hero's turn
    // guard (self), strike -> wolf, health_potion, ether, wait. No strike on the dead bandit.
    REQUIRE(legal.size() == 5);
    REQUIRE(legal[0].skill == "guard");
    REQUIRE(legal[1].skill == "strike");
    REQUIRE(legal[1].target == 1u);
    REQUIRE(legal[2].item == "health_potion");
    REQUIRE(legal.back().skill.empty());
    REQUIRE(legal.back().item.empty());
}

TEST_CASE("Turn-only fixture: a full AI-vs-AI encounter has a fixed result (regression)")
{
    Battle b;
    turn::TurnEncounter encounter(C(), b.Roster());
    const int actions = b.PlayOut(encounter);
    REQUIRE(encounter.IsOver());
    // No randomness: these exact numbers come back every run. If a rule changes on purpose,
    // update them and say why in the commit.
    CAPTURE(actions, encounter.Round(), b.hero.health, rpg::CountItem(b.hero, "health_potion"));
    REQUIRE(encounter.Winner() == 0);
    REQUIRE(actions == 26);
    REQUIRE(encounter.Round() == 12);
    REQUIRE(b.hero.health == 32);
    REQUIRE(rpg::CountItem(b.hero, "health_potion") == 0); // drank all three when low
}

TEST_CASE("The conventional enemy heals when low and otherwise attacks the weakest")
{
    Battle b;
    b.bandit.health = 10; // below 35% of 70
    b.hero.health = 30;
    turn::TurnEncounter encounter(C(), b.Roster());
    REQUIRE(encounter.Submit(Wait(1, 1)) == Rejection::None);
    REQUIRE(encounter.Submit(Wait(2, 0)) == Rejection::None);
    const rpg::ActionIntent choice = turn::ChooseTurnAction(C(), encounter); // bandit
    REQUIRE(choice.item == "health_potion");

    b.bandit.health = 70;
    b.hero.health = 110;
    b.wolf.health = 5;
    turn::TurnEncounter second(C(), b.Roster());
    REQUIRE(second.Submit(Wait(1, 1)) == Rejection::None);
    const rpg::ActionIntent heroChoice = turn::ChooseTurnAction(C(), second); // hero
    REQUIRE(heroChoice.skill == "strike");
    REQUIRE(heroChoice.target == 1u); // the nearly dead wolf
}
