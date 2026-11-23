#pragma once

#include <Diagnostic.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
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
    // Revive (a fallen ally stands up), Remove (a fallen foe leaves the fight): board (Match.md "Falls").
    enum class AbilityClass : std::uint8_t { Strike, Shield, Heal, Hit, FirstStrike, Revive, Remove };
    enum class Tier : std::uint8_t { Weak, Strong, Great, Guardian };
    enum class Behaviour : std::uint8_t { Stay, Flee, Stage, Guard, Hunt };
    // Who a board ability may aim at: the target symbol beside it on screen.
    enum class Target : std::uint8_t { Enemy, Ally, Self, Monster, Room, Any };
    enum class ItemClass : std::uint8_t { HealItem, ExpGrant, Revive }; // Revive: amount = percent of max HP
    enum class TreasureKind : std::uint8_t { Item, Essence, Xp, Heal };
    enum class CardKind : std::uint8_t { Learn, Stat, Upgrade };
    enum class Stat : std::uint8_t { Hp, Attack, Defence, Speed, Move, Power, Cost }; // Power, Cost: what an upgrade raises or cuts
    enum class Effect : std::uint8_t { None, Hp, Essence, Xp, Item };
    enum class BotLevel : std::uint8_t { Easy, Medium, Hard };

    struct Dice
    {
        int count = 0, sides = 0; // used instead of power when count > 0
    };

    enum class RangeBy : std::uint8_t { Walk, Straight }; // range by steps through doors, or squares counted straight (Manhattan)

    struct AbilityDef
    {
        std::string id, name, animation;
        Use use = Use::Battle;
        AbilityClass kind = AbilityClass::Strike;
        Target target = Target::Enemy;
        int hpCost = 0, power = 0, range = 0;
        RangeBy reach = RangeBy::Walk;
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
        int hp = 0, attack = 0, defence = 0, speed = 0, essence = 0, xp = 0;
        int level = 1, size = 1, sight = 0; // size: squares per side (1-3); sight: steps a hunter sees
        std::vector<int> abilities;
        std::uint32_t tint = 0xFFFFFF;
    };

    struct ItemDef
    {
        std::string id, name, text, sprite;
        ItemClass kind = ItemClass::HealItem;
        int amount = 0;
    };

    // One treasure.csv row: what a face-down card in an item room may hold, and how often (weight).
    struct TreasureDef
    {
        std::string id;
        TreasureKind kind = TreasureKind::Essence;
        int item = -1, amount = 0, weight = 1;
    };

    // One levelcards.csv row: a kind of card a level may offer, how often (weight), and for a stat card how much.
    struct LevelCardDef
    {
        std::string id;
        CardKind kind = CardKind::Stat;
        Stat what = Stat::Attack;
        int amount = 0, weight = 1;
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

    // One spawns.csv row: from round `from`, `monster` appears in `zone` (0 weak, 1 strong, 2 centre)
    // `every` rounds after its square empties.
    struct SpawnRow
    {
        int zone = 0, from = 1, monster = -1, every = 1;
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
            int width = 0, height = 0, lanes = 0, squareCapacity = 1, stride = 1; // stride: tiles a monster walks per point of speed
            int weakMin = 0, weakMax = 0, strongMin = 0, strongMax = 0, weakPerHalf = 0, strongPerHalf = 0;
            int eventsPerHalf = 0, eventGap = 0, itemsPerHalf = 0;
        } board;
        struct { int turns = 0; } respawn;
        struct { int rounds = 0, minDamage = 0; } battle;
        struct { std::vector<int> xp; int hp = 0, killXp = 0, abilityXp = 0, cards = 3, upgradePower = 2, upgradeCost = 1; } levels;
        struct { int min = 2, max = 5, fullEssence = 2; } treasure;
        struct { int respawn = 8; } rooms; // a claimed item or event room comes back this many rounds later // cards per item room; an item card with a full bag gives essence
        struct { int warn = 1, centre = 2; } spawn; // warn: rounds marked before; centre: steps from the stage
        int carry = 0;
        struct { std::array<int, 3> noise{}; } bots; // percent of a choice's value, per BotLevel
        struct { int sideMin = 0, sideMax = 0, firstMoverMax = 0, sourceMax = 0, comebackMin = 0; } balance;
    };

    // The switches of a fight (Match.md "Modes"): the faction match's and the campaign's, from
    // rules.json "modes"; a campaign stage may change them (plan 4).
    enum class WinBy : std::uint8_t { Score, Boss };
    struct Mode
    {
        bool essence = true, perform = true, respawn = true, fightXp = true; // the faction match's
        bool biding = false, fallsStay = false, playerLoss = false, mustFree = false;
        WinBy win = WinBy::Score;
        int roundLimit = 0; // 0 = none
        int maxAp = 2, apPerTurn = 2;
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
        std::vector<SpawnRow> spawns; // sorted by zone, then from
        std::vector<TreasureDef> treasure;
        std::vector<LevelCardDef> levelCards;
        std::vector<ItemDef> items;
        std::vector<EventDef> events;
        std::map<std::string, Mode> modes; // "faction" and "campaign"

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
