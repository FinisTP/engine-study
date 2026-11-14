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
    enum class RoomKind : std::uint8_t { Foyer, UnderStage, Plain, Item, Event, Trap, Omen, Transport, Bonus };
    enum class Effect : std::uint8_t { None, Hp, HpRoom, MaxHp, Might, Sanity, Speed, Item, Xp, Buff, Armor, Light, Revive };
    enum class ItemKind : std::uint8_t { Heal, Boost, Worn, Relic };
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
        struct { std::vector<int> targets; } omens;
        struct { int fromRound = 3, chancePercent = 35, stayRounds = 3, max = 2; } monsters;
        struct { int target = 13, bond = 3, retryRounds = 2; } persuade;
        struct { int downedReturnHp = 1, attackBondLoss = 300; } guests;
        struct { int companyWinMin = 40, companyWinMax = 60; } balance;
    };

    struct GuestDef
    {
        std::string id, name, pronoun;
        Weapon weapon = Weapon::Physical;
        int hp = 0, might = 0, sanity = 0, speed = 0;
        int style = 0;     // styles.csv row
        bool flees = false; // temper: fight or flee
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
    };

    struct OmenDef
    {
        std::string id, name;
        int might = 0, sanity = 0, speed = 0, maxhp = 0;
    };

    struct StyleDef
    {
        std::string id;
        int explore = 0, fight = 0, loot = 0, omen = 0, bonus = 0, fleeBelowHpPercent = 0, avoidGuests = 0;
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
        std::vector<MonsterDef> monsters;
        std::vector<OmenDef> omens;
        std::vector<StyleDef> styles;
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
