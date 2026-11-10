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
    // Content file format version read by LoadContent ("format": "umbra.rpg").
    inline constexpr int contentFormatVersion = 1;

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
        std::string effect; // particle effect over whoever has it (asset path; the game reads it, rules never do)
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
        std::string sound;                 // played when it hits (asset path; the game reads it, rules never do)
        std::string effect;                // particle effect where it hits (asset path; "" = the default spark)
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
        std::string sound;               // played when it is used (asset path)
        std::string effect;              // particle effect on whoever uses it (asset path)
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
        int xp = 0;         // experience the winner gets for defeating one
        StatBlock growth{}; // added to the base stats per level above 1
        // Look in the world (the game reads these; rules never do). Empty model = placeholder cube.
        std::string model;                       // asset path, e.g. "models/wolf.glb"
        std::string animation;                   // graph for a rigged model, e.g. "animations/wolf.anim.json" (S8)
        std::array<float, 4> color{1, 1, 1, 1};  // tint multiplied with the texture
        float size = 0.8F;                       // half-height in meters
        std::string policy;                      // learned turn-combat AI file (empty = conventional AI)
    };

    // --- Story: quests and conversations (P6). State lives in story::QuestLog. ---

    enum class QuestState
    {
        NotStarted,
        Active,
        Completed,
        Failed
    };

    enum class ObjectiveKind
    {
        Defeat, // defeat `count` actors with id `target`
        Obtain, // hold `count` items `target`
        Talk    // talk to the npc `target` (a conversation's "talk" action)
    };

    struct ObjectiveDef final
    {
        ObjectiveKind kind = ObjectiveKind::Talk;
        std::string target;
        int count = 1;
        std::string text; // shown in the quest log
    };

    // Objectives are done in order: only the current one makes progress.
    struct QuestDef final
    {
        std::string id;
        std::string name;
        std::string description;
        std::vector<ObjectiveDef> objectives;
        bool failOnDeath = false;
        int xp = 0;
        int skillPoints = 0;
        std::vector<ItemCount> items;
    };

    // A choice is shown only when all its conditions hold.
    struct Condition final
    {
        enum class Kind
        {
            QuestIs,
            HasItem
        };
        Kind kind = Kind::QuestIs;
        std::string id;
        QuestState state = QuestState::NotStarted;
        int count = 1;
    };

    // What picking a choice does. All of a choice's actions happen, or none (e.g. no room for an item).
    struct StoryAction final
    {
        enum class Kind
        {
            StartQuest,
            Talk, // counts for Talk objectives
            Give, // the npc gives items to the player
            Take  // the npc takes items from the player
        };
        Kind kind = Kind::Talk;
        std::string id;
        int count = 1;
    };

    struct DialogueChoice final
    {
        std::string text;
        std::string next; // node id; empty ends the conversation
        std::vector<Condition> conditions;
        std::vector<StoryAction> actions;
    };

    struct DialogueNode final
    {
        std::string speaker;
        std::string text;
        std::vector<DialogueChoice> choices; // none: the conversation ends after this line
    };

    struct ConversationDef final
    {
        std::string id;
        std::string npc; // who this conversation belongs to
        std::string start;
        std::map<std::string, DialogueNode> nodes;
    };

    struct Content final
    {
        std::map<std::string, StatusDef> statuses;
        std::map<std::string, SkillDef> skills;
        std::map<std::string, ItemDef> items;
        std::map<std::string, ActorDef> actors;
        std::map<std::string, QuestDef> quests;
        std::map<std::string, ConversationDef> conversations;

        [[nodiscard]] const StatusDef *FindStatus(const std::string &id) const;
        [[nodiscard]] const SkillDef *FindSkill(const std::string &id) const;
        [[nodiscard]] const ItemDef *FindItem(const std::string &id) const;
        [[nodiscard]] const ActorDef *FindActor(const std::string &id) const;
        [[nodiscard]] const QuestDef *FindQuest(const std::string &id) const;
        [[nodiscard]] const ConversationDef *FindConversation(const std::string &id) const;
    };

    [[nodiscard]] const char *StatName(Stat stat);
    [[nodiscard]] const char *ToString(QuestState state);

    // Parses and validates everything; on any error `content` is left untouched.
    bool LoadContent(const std::filesystem::path &file, Content &content, std::vector<core::Diagnostic> &diagnostics);
    bool ParseContent(const std::string &text, const std::string &fileName, Content &content,
                      std::vector<core::Diagnostic> &diagnostics);
}
