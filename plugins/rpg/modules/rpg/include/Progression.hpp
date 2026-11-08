#pragma once

#include <Character.hpp>

#include <vector>

namespace umbra::rpg
{
    inline constexpr int maxLevel = 20;

    // Total experience needed to reach `level`: 25 * level * (level - 1)  (0, 50, 150, 300, ...).
    [[nodiscard]] int XpForLevel(int level);

    // Adds experience; each level gained gives 1 skill point and restores health and energy to
    // the new maximum. Returns the number of levels gained.
    int GrantXp(const Content &content, Character &character, int amount);

    // Gives `items` to the character; whatever doesn't fit goes to `pending` (nothing is lost).
    void GiveItems(const Content &content, Character &character, const std::vector<ItemStack> &items);
    // Moves pending items into the inventory as far as room allows. Returns how many stacks moved.
    int ClaimPending(const Content &content, Character &character);

    struct Victory final
    {
        int xp = 0;
        int levels = 0;
        std::vector<ItemStack> loot; // everything the defeated carried
    };

    // After winning: the defeated enemies' experience and everything they carried go to the winner.
    // Call once per fight (the caller owns that rule: the encounter ends right after).
    Victory AwardVictory(const Content &content, Character &winner, const std::vector<const Character *> &defeated);
}
