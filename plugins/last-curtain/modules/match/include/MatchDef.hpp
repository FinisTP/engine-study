#pragma once

#include <Diagnostic.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// The faction match's data, read from plugins/last-curtain/assets/match/ (Match.md "Data").
// One struct per file, one member per column; ids become indexes after loading.
namespace umbra::match
{
    enum class Role : std::uint8_t { Attacker, Defender, Supporter, Speedster, AllRounder };
    enum class Use : std::uint8_t { Battle, Board, Passive };
    // Strike, Shield: battle. Heal: battle or board. Hit: board. FirstStrike: passive.
    enum class AbilityClass : std::uint8_t { Strike, Shield, Heal, Hit, FirstStrike };
    enum class Tier : std::uint8_t { Weak, Strong, Great, Guardian };
    enum class Behaviour : std::uint8_t { Stay, Flee, Stage, Guard };
    enum class ItemClass : std::uint8_t { HealItem, ExpGrant };
    enum class Effect : std::uint8_t { None, Hp, Essence, Xp, Item };
    enum class BotLevel : std::uint8_t { Easy, Medium, Hard };

    struct Dice
    {
        int count = 0, sides = 0; // used instead of power when count > 0
    };

    struct AbilityDef
    {
        std::string id, name, animation;
        Use use = Use::Battle;
        AbilityClass kind = AbilityClass::Strike;
        int hpCost = 0, power = 0, range = 0;
        Dice dice;
    };

    struct CharacterDef
    {
        std::string id, name, sprite;
        Role role = Role::Attacker;
        int hp = 0, attack = 0, defence = 0, speed = 0, move = 0;
        std::uint32_t tint = 0xFFFFFF;
    };

    // Level 1 rows are the starting abilities; a choice is a second ability to pick between.
    struct JobLevel
    {
        Role role = Role::Attacker;
        int level = 1, ability = -1, choice = -1;
    };

    struct MonsterDef
    {
        std::string id, name, sprite;
        Tier tier = Tier::Weak;
        Behaviour behaviour = Behaviour::Stay;
        int hp = 0, attack = 0, defence = 0, speed = 0, essence = 0, xp = 0, respawnRounds = 0;
        std::vector<int> abilities;
        std::uint32_t tint = 0xFFFFFF;
    };

    struct ItemDef
    {
        std::string id, name, text, sprite;
        ItemClass kind = ItemClass::HealItem;
        int amount = 0;
    };

    struct Outcome
    {
        Effect effect = Effect::None;
        int amount = 0; // Item: the items.csv row
    };

    struct Choice
    {
        std::string label;
        int chance = 0; // percent
        Outcome success, failure;
    };

    struct EventDef
    {
        std::string id, name, text;
        std::vector<Choice> choices;
    };

    struct Options
    {
        int rounds = 30, side = 3;
        bool autoEndTurn = false;
        BotLevel bots = BotLevel::Medium;
    };

    struct Rules
    {
        struct
        {
            int width = 0, height = 0, lanes = 0, squareCapacity = 1;
            int weakMin = 0, weakMax = 0, strongMin = 0, strongMax = 0, weakPerHalf = 0, strongPerHalf = 0;
            int eventsPerHalf = 0, eventGap = 0, itemsPerHalf = 0;
        } board;
        struct { int openRounds = 0; } stage;
        struct { int turns = 0; } respawn;
        struct { int rounds = 0, minDamage = 0; } battle;
        struct { std::vector<int> xp; int hp = 0, killXp = 0; } levels;
        struct { int wakeRound = 0; } great;
        int carry = 0;
        struct { std::array<int, 3> noise{}; } bots; // percent of a choice's value, per BotLevel
        struct { int sideMin = 0, sideMax = 0, firstMoverMax = 0, sourceMax = 0, comebackMin = 0; } balance;
    };

    inline constexpr int maxAbilities = 8, maxItems = 4; // slots in a Unit; rules.carry may not exceed maxItems

    struct MatchDef
    {
        Options options;
        Rules rules;
        std::vector<CharacterDef> characters;
        std::vector<JobLevel> jobs;
        std::vector<AbilityDef> abilities;
        std::vector<MonsterDef> monsters;
        std::vector<ItemDef> items;
        std::vector<EventDef> events;

        // The index of the row with this id, or -1.
        template <class T> static int Find(const std::vector<T> &rows, std::string_view id)
        {
            for (std::size_t i = 0; i < rows.size(); ++i)
                if (rows[i].id == id)
                    return static_cast<int>(i);
            return -1;
        }
    };

    // Loads and checks every file of the folder. Diagnostics: code "match.<file>", field "<file>:<row>:<column>".
    bool LoadMatch(const std::filesystem::path &folder, MatchDef &out, std::vector<core::Diagnostic> &diagnostics);
}
