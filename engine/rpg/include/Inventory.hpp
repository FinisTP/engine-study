#pragma once

#include <Character.hpp>

namespace umbra::rpg
{
    // Inventory and equipment transactions. Each one either fully succeeds or changes nothing.

    // Tops up existing stacks, then fills free slots. If everything doesn't fit, nothing is
    // added (user decision: a full inventory rejects the pickup and the item stays where it was).
    bool AddItem(const Content &content, Character &character, const std::string &item, int count);

    // Removes `count` items (from the last stacks first). False and no change if there are fewer.
    bool RemoveItem(Character &character, const std::string &item, int count);
    [[nodiscard]] int CountItem(const Character &character, const std::string &item);

    // Moves one item from the inventory into its slot; an item already there goes back into the
    // inventory (it takes the freed place, so this never overflows). Health/energy are clamped.
    bool Equip(const Content &content, Character &character, const std::string &item);
    // Moves the slot's item into the inventory. False and no change if the inventory is full.
    bool Unequip(const Content &content, Character &character, Slot slot);
}
