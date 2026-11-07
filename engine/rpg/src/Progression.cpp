#include <Progression.hpp>

#include <Inventory.hpp>

#include <algorithm>

namespace umbra::rpg
{
    int XpForLevel(int level) { return 25 * level * (level - 1); }

    int GrantXp(const Content &content, Character &character, int amount)
    {
        // 1. Experience stops growing at the last level.
        character.xp = std::min(character.xp + std::max(0, amount), XpForLevel(maxLevel));
        int gained = 0;
        while (character.level < maxLevel && character.xp >= XpForLevel(character.level + 1))
        {
            ++character.level;
            ++character.skillPoints;
            ++gained;
        }
        // 2. A level up is a reward: back to full health and energy at the new maximum.
        if (gained > 0)
        {
            const StatBlock stats = DerivedStats(content, character);
            character.health = Get(stats, Stat::MaxHealth);
            character.energy = Get(stats, Stat::MaxEnergy);
        }
        return gained;
    }

    void GiveItems(const Content &content, Character &character, const std::vector<ItemStack> &items)
    {
        for (const ItemStack &stack : items)
        {
            if (!AddItem(content, character, stack.item, stack.count))
                character.pending.push_back(stack);
        }
    }

    int ClaimPending(const Content &content, Character &character)
    {
        int moved = 0;
        std::vector<ItemStack> left;
        for (const ItemStack &stack : character.pending)
        {
            if (AddItem(content, character, stack.item, stack.count))
                ++moved;
            else
                left.push_back(stack);
        }
        character.pending = std::move(left);
        return moved;
    }

    Victory AwardVictory(const Content &content, Character &winner, const std::vector<const Character *> &defeated)
    {
        Victory victory;
        for (const Character *enemy : defeated)
        {
            victory.xp += content.FindActor(enemy->actor)->xp;
            victory.loot.insert(victory.loot.end(), enemy->inventory.begin(), enemy->inventory.end());
        }
        victory.levels = GrantXp(content, winner, victory.xp);
        GiveItems(content, winner, victory.loot);
        return victory;
    }
}
