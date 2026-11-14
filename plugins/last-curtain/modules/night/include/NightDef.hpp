#pragma once

#include <Diagnostic.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// The night's data, read from plugins/last-curtain/assets/night/ (Night.md "Data").
// One struct per file, one member per column; ids become indexes after loading.
namespace umbra::night
{
    enum class StatKind : std::uint8_t { Might, Sanity, Speed, MaxHp, Light, None };
    enum class Weapon : std::uint8_t { Physical, Magical };
    enum class RoomKind : std::uint8_t { Foyer, UnderStage, Plain, Item, Event, Trap, Transport, Bonus };
    enum class Effect : std::uint8_t { None, Hp, HpRoom, MaxHp, Might, Sanity, Speed, Item, Xp, Buff, Armor, Light, Revive, Back };
    enum class ItemKind : std::uint8_t { Heal, Boost, Worn, Relic, Keepsake };
    enum class GoalKind : std::uint8_t { Room, Item, Events, Monsters }; // no ally goals (user)
    enum class BuffStat : std::uint8_t { Guard, Might, Sanity };
    enum class PowerKind : std::uint8_t { Line, Area, Flood, Pull, Seal, Swap, Reflect };

    struct Rules
    {
        struct { int startHour = 19, rounds = 20, turnAfterRound = 10; } clock;
        struct { int width = 5, height = 5, rooms = 20; } board;
        struct { int easy = 10, normal = 13, hard = 16; } checks;
        struct { int steps = 4; } light;
        int carry = 4;
        struct { int rounds = 3, minDamage = 1, base = 3, defenseDivisor = 2, guardDivisor = 2; } battle;
        struct { std::vector<int> xp; int hp = 4, stat = 1; } levels;
        struct { int fromRound = 3, chancePercent = 35, stayRounds = 3, max = 2; } monsters;
        struct { int failHp = 1, earlyRooms = 35, roomGoalRooms = 65; } goals; // HP a failed guest keeps at midnight; keepsake / goal rooms come out within this many
        struct { int approvals = 3, bond = 3; } persuade; // approvals that make an ally; the bond's pull on a target
        struct { int downedReturnHp = 1, attackBondLoss = 300; } guests;
        struct
        {
            int companyWinMin = 40, companyWinMax = 60; // per Taken form, % of hunts
            int takenMin = 5, takenMax = 30;            // per guest, % of nights it is the Taken
            int allFinishedMax = 10;                    // % of nights with no hunt
            int roomsMin = 45, roomsMax = 65;           // rooms open by midnight, a night on average
            int botChatRoundsMin = 70;                  // % of evening rounds with a chat between bots
            int goalMin = 45, goalMax = 65;             // per goal, % of the nights it is drawn
        } balance;
    };

    struct GuestDef
    {
        std::string id, name, pronoun;
        Weapon weapon = Weapon::Physical;
        int hp = 0, might = 0, sanity = 0, speed = 0;
        int style = 0; // styles.csv row
        std::string taken;
        int takenHp = 0, takenMight = 0, takenSanity = 0, takenSpeed = 0;
        int power = 0; // powers.json entry
    };

    struct BondDef { int from = 0, to = 0, bond = 0; };

    struct PowerDef
    {
        std::string id;
        PowerKind kind = PowerKind::Line;
        int damage = 0, range = 0, rounds = 0, cooldown = 1;
    };

    struct RoomDef
    {
        std::string id, name;
        RoomKind kind = RoomKind::Plain;
        std::uint8_t doors = 0; // NESW mask, bit 0 = N
        StatKind stat = StatKind::None;
        int amount = 0, count = 0;
    };

    struct EventDef
    {
        std::string id, name;
        bool trap = false;
        StatKind stat = StatKind::Might;
        int target = 0; // the number, from rules.checks
        Effect passEffect = Effect::None, failEffect = Effect::None;
        int passAmount = 0, failAmount = 0;
        std::string text; // a short story for the screen
    };

    struct ItemDef
    {
        std::string id, name;
        ItemKind kind = ItemKind::Heal;
        Effect effect = Effect::None;
        int amount = 0;
        int buff = -1; // buffs.csv row
        int weight = 1;
        std::string text;
        int room = -1; // a keepsake: the rooms.csv row where it waits
    };
    // A reason a guest came tonight (goals.csv). target: a rooms.csv, items.csv or guests.csv row; -1 for events and monsters.
    struct GoalDef
    {
        int guest = 0;
        std::string id;
        GoalKind kind = GoalKind::Room;
        int target = -1, count = 1;
        std::string text;
    };

    struct BuffDef
    {
        std::string id, name;
        BuffStat stat = BuffStat::Guard;
        int amount = 0, rounds = 0; // rounds 0: until hit (guard) or the battle's end
    };

    struct MonsterDef
    {
        std::string id, name;
        int hp = 0, might = 0, sanity = 0;
        Weapon attack = Weapon::Physical;
        int resist = -1; // a Weapon value or -1
        int xp = 0;
        int debuff = -1; // buffs.csv row
        int hpPerHour = 0, statPerHour = 0;
        int taken = -1;  // a minion: the guests.csv row of its Taken (minions.csv); -1 = an ordinary monster
        int hpShare = 0; // a minion: the share of its Taken's max HP (percent) that dies with it
        int count = 0;   // a minion: how many are placed at the Turn
    };

    // One of a guest's questions: two answers, one physical and one magical, each with its target.
    struct QuestionDef
    {
        int guest = 0; // guests.csv row
        std::string id, text, physicalText, magicalText;
        int physicalTarget = 0, magicalTarget = 0;
    };


    struct StyleDef
    {
        std::string id;
        int explore = 0, fight = 0, loot = 0, bonus = 0, fleeBelowHpPercent = 0, avoidGuests = 0;
        int healBelowPercent = 40; // in battle, heal below this share of max HP
        int huntHpPercent = 50;    // in the hunt, go for the Taken only above this share of max HP
        int huntNearSteps = 2;     // ... and with another guest this near
        int crowd = 3;             // the Taken keeps away from this many guests together
        int chatty = 0;            // how readily it talks (Balance.md "The bots"; 6+: before moving once its goal is done, 7+: asks the player)
        int greed = 10;            // 0: picks up only healing and needed keepsakes; 10: everything
    };

    struct NightDef
    {
        Rules rules;
        std::vector<GuestDef> guests;
        std::vector<BondDef> bonds;
        std::vector<PowerDef> powers;
        std::vector<RoomDef> rooms;
        std::vector<EventDef> events;
        std::vector<ItemDef> items;
        std::vector<BuffDef> buffs;
        std::vector<MonsterDef> monsters; // ordinary monsters, then the minions of minions.csv
        std::vector<StyleDef> styles;
        std::vector<QuestionDef> questions; // five per guest
        std::vector<GoalDef> goals;         // two or three per guest
        std::uint64_t fingerprint = 0; // a hash of every data file, so saves match their data

        // The index of the row with this id, or -1.
        template <class T> static int Find(const std::vector<T> &rows, std::string_view id)
        {
            for (std::size_t i = 0; i < rows.size(); ++i)
                if (rows[i].id == id)
                    return static_cast<int>(i);
            return -1;
        }
    };

    inline constexpr int maxCarry = 6; // item slots in a Guest; rules.carry may not exceed it

    // Loads and checks every file of the folder. Diagnostics: code "night.<file>", field "<file>:<row>:<column>".
    bool LoadNight(const std::filesystem::path &folder, NightDef &out, std::vector<core::Diagnostic> &diagnostics);
}
