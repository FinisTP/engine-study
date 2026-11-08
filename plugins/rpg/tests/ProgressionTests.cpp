#include "RpgTestContent.hpp"

#include <Inventory.hpp>
#include <Progression.hpp>

using namespace umbra::rpg;

TEST_CASE("Levels follow the experience curve and add growth and a skill point")
{
    REQUIRE(XpForLevel(1) == 0);
    REQUIRE(XpForLevel(2) == 50);
    REQUIRE(XpForLevel(3) == 150);

    Character hero = rpgtest::Make("hero", "hero");
    hero.health = 10;
    REQUIRE(GrantXp(rpgtest::Reference(), hero, 49) == 0);
    REQUIRE(hero.health == 10);
    REQUIRE(GrantXp(rpgtest::Reference(), hero, 101) == 2); // 150 xp = level 3
    REQUIRE(hero.level == 3);
    REQUIRE(hero.skillPoints == 3 + 2);
    const StatBlock stats = DerivedStats(rpgtest::Reference(), hero);
    REQUIRE(Get(stats, Stat::MaxHealth) == 110 + 2 * 10); // growth 10 per level
    REQUIRE(Get(stats, Stat::Attack) == 14 + 2 * 2);
    REQUIRE(hero.health == Get(stats, Stat::MaxHealth)); // a level up restores health

    GrantXp(rpgtest::Reference(), hero, 1'000'000);
    REQUIRE(hero.level == maxLevel);
    REQUIRE(hero.xp == XpForLevel(maxLevel)); // capped
}

TEST_CASE("Victory gives the enemies' experience and everything they carried, once")
{
    Character hero = rpgtest::Make("hero", "hero");
    const Character leader = rpgtest::Make("bandit_leader", "leader");
    const Character wolf = rpgtest::Make("wolf", "wolf_1");
    const Victory v = AwardVictory(rpgtest::Reference(), hero, {&leader, &wolf});
    REQUIRE(v.xp == 60 + 15);
    REQUIRE(v.levels == 1);
    REQUIRE(CountItem(hero, "sun_relic") == 1);
    REQUIRE(hero.pending.empty());
}

TEST_CASE("Loot that doesn't fit waits in pending and is claimed when there is room")
{
    Character hero = rpgtest::Make("hero", "hero");
    hero.inventorySlots = static_cast<int>(hero.inventory.size()); // full
    const Character leader = rpgtest::Make("bandit_leader", "leader");
    AwardVictory(rpgtest::Reference(), hero, {&leader});
    REQUIRE(CountItem(hero, "sun_relic") == 0);
    REQUIRE(hero.pending.size() == 1); // not lost (user decision: rejected pickups stay)
    REQUIRE(ClaimPending(rpgtest::Reference(), hero) == 0);
    hero.inventorySlots += 1;
    REQUIRE(ClaimPending(rpgtest::Reference(), hero) == 1);
    REQUIRE(CountItem(hero, "sun_relic") == 1);
    REQUIRE(hero.pending.empty());
}
