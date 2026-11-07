#pragma once

#include <Diagnostic.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace umbra::rpg
{
    // Immutable RPG definitions loaded from a versioned JSON file (format in Rpg.md).
    // Definitions never change during play; changing state lives in Character.

    enum class Stat
    {
        MaxHealth,
        MaxEnergy,
        Attack,
        Defense,
        Speed,
        Count
    };
    using StatBlock = std::array<int, static_cast<std::size_t>(Stat::Count)>;

    // Applied as (base + sum of add) * (100 + sum of percent) / 100. See Rpg.md.
    struct Modifier final
    {
        Stat stat = Stat::Attack;
        int add = 0;
        int percent = 0;
    };

    // A status lasts a number of turns (turn combat) and/or seconds (real-time combat).
    // The two clocks are separate on purpose: nothing converts turns into seconds.
    struct StatusDef final
    {
        std::string id;
        std::string name;
        std::optional<int> turns;
        std::optional<float> seconds;
        int damagePerTick = 0; // per turn or per second; negative heals
        std::vector<Modifier> modifiers;
    };

    enum class EffectKind
    {
        Damage,
        Heal,
        RestoreEnergy,
        ApplyStatus
    };

    struct EffectDef final
    {
        EffectKind kind = EffectKind::Damage;
        int power = 0;      // damage, heal or energy amount
        std::string status; // for ApplyStatus
    };

    enum class TargetRule
    {
        Enemy,
        Self
    };

    // Real-time attack phases in seconds, and where it hits.
    struct RealtimeTiming final
    {
        float windup = 0.0F;
        float active = 0.0F;
        float recovery = 0.0F;
        float cooldown = 0.0F;
        float range = 0.0F;        // meters
        float arcDegrees = 90.0F;  // full cone width in front of the attacker
    };

    struct SkillDef final
    {
        std::string id;
        std::string name;
        int energyCost = 0;
        TargetRule target = TargetRule::Enemy;
        std::vector<EffectDef> effects;
        bool turn = false;     // usable in turn combat
        bool realtime = false; // usable in real-time combat (needs timing)
        RealtimeTiming timing;
        int pointCost = 0;                 // skill points to unlock
        std::vector<std::string> prerequisites; // skills that must be unlocked first ("requires" in JSON)
    };

    enum class Slot
    {
        None,
        Weapon,
        Armor
    };

    struct ItemDef final
    {
        std::string id;
        std::string name;
        Slot slot = Slot::None;
        int maxStack = 1;
        std::vector<Modifier> modifiers; // while equipped
        std::vector<EffectDef> use;      // consumed on use (on self) when not empty
    };

    struct ItemCount final
    {
        std::string item;
        int count = 1;
    };

    // An archetype: one definition can be placed many times (e.g. several wolves).
    struct ActorDef final
    {
        std::string id;
        std::string name;
        StatBlock base{};
        std::vector<std::string> skills;    // unlocked from the start
        std::vector<std::string> equipment; // equipped from the start
        std::vector<ItemCount> inventory;
        int inventorySlots = 12;
        int skillPoints = 0;
    };

    struct Content final
    {
        std::map<std::string, StatusDef> statuses;
        std::map<std::string, SkillDef> skills;
        std::map<std::string, ItemDef> items;
        std::map<std::string, ActorDef> actors;

        [[nodiscard]] const StatusDef *FindStatus(const std::string &id) const;
        [[nodiscard]] const SkillDef *FindSkill(const std::string &id) const;
        [[nodiscard]] const ItemDef *FindItem(const std::string &id) const;
        [[nodiscard]] const ActorDef *FindActor(const std::string &id) const;
    };

    [[nodiscard]] const char *StatName(Stat stat);

    // Parses and validates everything; on any error `content` is left untouched.
    bool LoadContent(const std::filesystem::path &file, Content &content, std::vector<core::Diagnostic> &diagnostics);
    bool ParseContent(const std::string &text, const std::string &fileName, Content &content,
                      std::vector<core::Diagnostic> &diagnostics);
}
