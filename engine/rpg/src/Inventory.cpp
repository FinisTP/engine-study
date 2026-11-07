#include <Inventory.hpp>

#include <algorithm>

namespace umbra::rpg
{
    bool AddItem(const Content &content, Character &character, const std::string &item, int count)
    {
        const ItemDef *def = content.FindItem(item);
        if (def == nullptr || count <= 0)
            return false;

        // Work on a copy so a failed add leaves the inventory exactly as it was.
        std::vector<ItemStack> inventory = character.inventory;
        for (ItemStack &stack : inventory)
        {
            if (stack.item != item)
                continue;
            const int moved = std::min(count, def->maxStack - stack.count);
            stack.count += moved;
            count -= moved;
        }
        while (count > 0 && static_cast<int>(inventory.size()) < character.inventorySlots)
        {
            const int moved = std::min(count, def->maxStack);
            inventory.push_back({item, moved});
            count -= moved;
        }
        if (count > 0)
            return false; // full
        character.inventory = std::move(inventory);
        return true;
    }
}
