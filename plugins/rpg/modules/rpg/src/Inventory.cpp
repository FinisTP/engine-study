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

    int CountItem(const Character &character, const std::string &item)
    {
        int count = 0;
        for (const ItemStack &stack : character.inventory)
            count += stack.item == item ? stack.count : 0;
        return count;
    }

    bool RemoveItem(Character &character, const std::string &item, int count)
    {
        if (count <= 0 || CountItem(character, item) < count)
            return false;
        // Take from the back so earlier stacks keep their places; drop emptied stacks.
        for (auto it = character.inventory.rbegin(); it != character.inventory.rend() && count > 0; ++it)
        {
            if (it->item != item)
                continue;
            const int taken = std::min(count, it->count);
            it->count -= taken;
            count -= taken;
        }
        std::erase_if(character.inventory, [](const ItemStack &stack) { return stack.count == 0; });
        return true;
    }

    bool Equip(const Content &content, Character &character, const std::string &item)
    {
        const ItemDef *def = content.FindItem(item);
        if (def == nullptr || def->slot == Slot::None || CountItem(character, item) == 0)
            return false;

        // 1. Take the new item out (equipment doesn't stack, so this frees a whole slot).
        Character changed = character;
        RemoveItem(changed, item, 1);
        // 2. The old item goes back into the freed place.
        if (const auto old = changed.equipment.find(def->slot); old != changed.equipment.end())
            changed.inventory.push_back({old->second, 1});
        changed.equipment[def->slot] = item;
        ClampResources(content, changed);
        character = std::move(changed);
        return true;
    }

    bool Unequip(const Content &content, Character &character, Slot slot)
    {
        const auto equipped = character.equipment.find(slot);
        if (equipped == character.equipment.end())
            return false;
        Character changed = character;
        changed.equipment.erase(slot);
        if (!AddItem(content, changed, equipped->second, 1))
            return false; // full: the item stays equipped
        ClampResources(content, changed);
        character = std::move(changed);
        return true;
    }
}
