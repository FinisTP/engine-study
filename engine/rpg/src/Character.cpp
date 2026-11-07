#include <Character.hpp>
#include <Inventory.hpp>

#include <algorithm>

namespace umbra::rpg
{
    std::optional<Character> MakeCharacter(const Content &content, const std::string &actorId, const std::string &instanceId)
    {
        const ActorDef *actor = content.FindActor(actorId);
        if (actor == nullptr)
            return std::nullopt;

        Character c;
        c.id = instanceId;
        c.actor = actorId;
        c.inventorySlots = actor->inventorySlots;
        c.skills.insert(actor->skills.begin(), actor->skills.end());
        c.skillPoints = actor->skillPoints;
        for (const std::string &item : actor->equipment)
            c.equipment[content.FindItem(item)->slot] = item; // validated on load
        for (const ItemCount &entry : actor->inventory)
        {
            if (!AddItem(content, c, entry.item, entry.count))
                return std::nullopt;
        }
        const StatBlock stats = DerivedStats(content, c);
        c.health = Get(stats, Stat::MaxHealth);
        c.energy = Get(stats, Stat::MaxEnergy);
        return c;
    }

    StatBlock DerivedStats(const Content &content, const Character &character)
    {
        // 1. Sum every flat and percent modifier per stat (order doesn't matter: sums commute).
        StatBlock add{}, percent{};
        const auto apply = [&](const std::vector<Modifier> &modifiers) {
            for (const Modifier &m : modifiers)
            {
                add[static_cast<std::size_t>(m.stat)] += m.add;
                percent[static_cast<std::size_t>(m.stat)] += m.percent;
            }
        };
        for (const auto &[slot, item] : character.equipment)
            apply(content.FindItem(item)->modifiers);
        for (const ActiveStatus &status : character.statuses)
            apply(content.FindStatus(status.status)->modifiers);

        // 2. (base + add) * (100 + percent) / 100, never below 0 (max health never below 1).
        StatBlock result = content.FindActor(character.actor)->base;
        for (std::size_t i = 0; i < result.size(); ++i)
        {
            const long long value = (static_cast<long long>(result[i]) + add[i]) * std::max(0, 100 + percent[i]) / 100;
            result[i] = static_cast<int>(std::clamp<long long>(value, i == 0 ? 1 : 0, 1'000'000));
        }
        return result;
    }

    void ClampResources(const Content &content, Character &character)
    {
        const StatBlock stats = DerivedStats(content, character);
        character.health = std::clamp(character.health, 0, Get(stats, Stat::MaxHealth));
        character.energy = std::clamp(character.energy, 0, Get(stats, Stat::MaxEnergy));
    }
}
