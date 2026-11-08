#pragma once

#include <RpgContent.hpp>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace umbra::rpg
{
    struct ItemStack final
    {
        std::string item;
        int count = 1;
    };

    // A status on a character. Only the clock of the combat mode that applied it is used.
    struct ActiveStatus final
    {
        std::string status;
        int turnsLeft = 0;
        float secondsLeft = 0.0F;
        float tickTimer = 0.0F; // seconds until the next per-second tick
    };

    // Changing state of one placed actor. Definitions are looked up by id, never copied in,
    // so this can be saved by ids and counts alone (no pointers).
    struct Character final
    {
        std::string id;    // durable instance id, e.g. "wolf_1"
        std::string actor; // ActorDef id
        int health = 0;
        int energy = 0;
        std::vector<ItemStack> inventory; // one entry per occupied slot
        int inventorySlots = 0;
        std::map<Slot, std::string> equipment;
        std::set<std::string> skills; // unlocked
        int skillPoints = 0;
        std::vector<ActiveStatus> statuses;
        int level = 1;
        int xp = 0;                     // total experience
        std::vector<ItemStack> pending; // items won with no room yet (claimed by ClaimPending)

        [[nodiscard]] bool IsAlive() const noexcept { return health > 0; }
    };

    // A fresh instance of an archetype at full health and energy. Nothing if the actor is
    // unknown or its starting inventory does not fit.
    [[nodiscard]] std::optional<Character> MakeCharacter(const Content &content, const std::string &actorId,
                                                         const std::string &instanceId);

    // Base stats plus equipment and status modifiers (formula in Rpg.md).
    [[nodiscard]] StatBlock DerivedStats(const Content &content, const Character &character);
    [[nodiscard]] inline int Get(const StatBlock &stats, Stat stat) { return stats[static_cast<std::size_t>(stat)]; }

    // Keeps health and energy within 0..max after a maximum changed. Never heals.
    void ClampResources(const Content &content, Character &character);
}
