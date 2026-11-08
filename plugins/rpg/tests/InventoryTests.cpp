#include "RpgTestContent.hpp"

#include <Inventory.hpp>

using namespace umbra::rpg;

namespace
{
    const Content &C() { return rpgtest::Reference(); }
}

TEST_CASE("Adding tops up stacks before using new slots")
{
    Character hero = rpgtest::Make("hero", "hero"); // 3 potions, 1 ether, 12 slots
    REQUIRE(AddItem(C(), hero, "health_potion", 4)); // 3 -> 5, then a new stack of 2
    REQUIRE(CountItem(hero, "health_potion") == 7);
    REQUIRE(hero.inventory.size() == 3);
    REQUIRE_FALSE(AddItem(C(), hero, "no_such_item", 1));
    REQUIRE_FALSE(AddItem(C(), hero, "ether", 0));
}

TEST_CASE("A full inventory rejects the whole pickup and changes nothing")
{
    Character bandit = rpgtest::Make("bandit", "bandit"); // 4 slots, 1 potion
    REQUIRE(AddItem(C(), bandit, "iron_sword", 1));
    REQUIRE(AddItem(C(), bandit, "leather_armor", 1));
    const auto before = bandit.inventory;
    // 10 potions: 4 top up the stack, 6 more need two new slots; only one is free.
    REQUIRE_FALSE(AddItem(C(), bandit, "health_potion", 10));
    REQUIRE(bandit.inventory.size() == before.size());
    REQUIRE(CountItem(bandit, "health_potion") == 1);
    REQUIRE(AddItem(C(), bandit, "health_potion", 4)); // fills the existing stack exactly
    REQUIRE(AddItem(C(), bandit, "iron_sword", 1));       // last free slot
    REQUIRE_FALSE(AddItem(C(), bandit, "iron_sword", 1)); // full
}

TEST_CASE("Removing more than owned fails without touching anything")
{
    Character hero = rpgtest::Make("hero", "hero");
    REQUIRE(AddItem(C(), hero, "health_potion", 4)); // stacks 5 + 2
    REQUIRE_FALSE(RemoveItem(hero, "health_potion", 8));
    REQUIRE(CountItem(hero, "health_potion") == 7);
    REQUIRE(RemoveItem(hero, "health_potion", 3)); // 2 from the last stack, 1 from the first
    REQUIRE(CountItem(hero, "health_potion") == 4);
    REQUIRE(RemoveItem(hero, "ether", 1));
    REQUIRE(hero.inventory.size() == 1); // emptied stacks are dropped
}

TEST_CASE("Equipping swaps items without duplicates or lost items")
{
    Character hero = rpgtest::Make("hero", "hero");
    REQUIRE(AddItem(C(), hero, "iron_sword", 1));
    REQUIRE(Equip(C(), hero, "iron_sword"));
    REQUIRE(hero.equipment.at(Slot::Weapon) == "iron_sword");
    REQUIRE(CountItem(hero, "iron_sword") == 0);
    REQUIRE(CountItem(hero, "rusty_sword") == 1); // old weapon back in the bag
    REQUIRE(Get(DerivedStats(C(), hero), Stat::Attack) == 18); // bonus applied once

    REQUIRE_FALSE(Equip(C(), hero, "iron_sword"));    // not in the inventory any more
    REQUIRE_FALSE(Equip(C(), hero, "health_potion")); // not equipment
    REQUIRE(Equip(C(), hero, "rusty_sword"));
    REQUIRE(Get(DerivedStats(C(), hero), Stat::Attack) == 14);
}

TEST_CASE("Unequipping into a full inventory is rejected; max health changes clamp (T11)")
{
    Character bandit = rpgtest::Make("bandit", "bandit");
    REQUIRE(AddItem(C(), bandit, "leather_armor", 1));
    REQUIRE(Equip(C(), bandit, "leather_armor"));
    REQUIRE(bandit.health == 70); // max rose to 80 but equipping doesn't heal

    bandit.inventorySlots = 1; // only the potion stack fits now
    REQUIRE_FALSE(Unequip(C(), bandit, Slot::Weapon));
    REQUIRE(bandit.equipment.contains(Slot::Weapon));

    bandit.inventorySlots = 4;
    bandit.health = 80;
    REQUIRE(Unequip(C(), bandit, Slot::Armor));
    REQUIRE(bandit.health == 70); // cut to the new maximum
    REQUIRE_FALSE(Unequip(C(), bandit, Slot::Armor)); // nothing there
}
