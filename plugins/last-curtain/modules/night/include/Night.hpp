#pragma once

#include <NightDef.hpp>

#include <array>
#include <cstdint>
#include <vector>

// The night as one copyable value and the rules that change it (Night.md).
namespace umbra::night
{
    inline constexpr int maxGuests = 7, maxCells = 49;

    // One square of the board. `room` is a rooms.csv row; `doors` the rotated NESW mask, bit 0 = N.
    struct Cell
    {
        std::int8_t room = -1;
        std::uint8_t doors = 0;
        bool opened = false;
        bool resolved = false; // its item, event, trap or omen is taken care of
        std::int8_t omen = -1; // the omens.csv row lying here (drawn on first entry)
    };

    struct Guest
    {
        std::int16_t hp = 0, maxHp = 0, might = 0, sanity = 0, speed = 0; // base values (levels, rooms, events)
        std::int8_t cell = -1, level = 1, group = -1;
        std::int16_t xp = 0;
        std::array<std::int8_t, maxCarry> items{-1, -1, -1, -1, -1, -1}; // items.csv rows, -1 = empty
        std::uint8_t omens = 0;        // omens.csv rows held, as bits
        std::uint64_t bonusUsed = 0;   // cells whose bonus this guest has had, as bits
        bool down = false, dead = false;
        std::int8_t downedBy = -1; // the guest who brought them down; -1 = the house
        std::int8_t pending = 0;   // levels earned and not yet chosen
        std::array<std::int8_t, maxGuests> askedUntilRound{}; // may not persuade that guest again before
        std::int8_t movesLeft = 0;
    };

    struct FloorItem
    {
        std::int8_t cell = -1, item = -1;
    };

    enum class ActionKind : std::uint8_t
    {
        Move, UseItem, DropItem, PickUp, EndTurn,
        TakeOmen, LeaveOmen, Persuade, Attack, Strike, Guard, Leave, LevelUp, Power
    };

    // Move: a = direction 0-3, or transportMove. UseItem/DropItem: a = slot (relic: b = the downed guest).
    // PickUp: a = index in `floor`. Attack: a = monster index or -1, b = guest or -1. Strike: a = fighter index.
    // Power: a = target guest (line, pull, swap) or cell (seal, with b = direction); -1 for area, flood, reflect.
    // LevelUp: a = 0 Might, 1 Sanity. Persuade: a = guest.
    inline constexpr std::int8_t transportMove = 4;
    struct Action
    {
        ActionKind kind = ActionKind::EndTurn;
        std::int8_t a = -1, b = -1;
        bool operator==(const Action &) const = default;
    };

    enum class LogKind : std::uint8_t
    {
        Opened, Check, Effect, Item, Down, Woke,
        Battle, Hit, Omen, Turned, Power, Died, Ended, Monster, Level, Persuade
    };

    // One thing that happened. Check: a = die, other = stat, b = target. Effect: a = Effect, b = amount.
    // Item: a = item. Down/Died: other = by whom (-1 house). Woke: other = helper. Opened: a = cell.
    // Monster: a = monsters.csv row (minions after the monsters); other -1 = appeared (b = cell), other 1 = killed by `who`.
    // Battle: who/other = first guest of each side.
    // Hit: who = striker (-1 monster), other = struck (-1 monster), a = damage, b = HP left. Ended: a = Outcome.
    struct LogEntry
    {
        std::int16_t tick = 0; // round * 8 + turn
        LogKind kind = LogKind::Opened;
        std::int8_t who = -1, other = -1;
        std::int16_t a = 0, b = 0;
    };

    struct Check
    {
        int die = 0, stat = 0, target = 0;
        bool passed = false;
    };

    enum class Outcome : std::uint8_t { Running, Company, Taken };

    // A flooded cell, or a sealed passage (cell + direction), until the end of a round.
    struct Mark
    {
        std::int8_t cell = -1, direction = -1, untilRound = 0;
    };

    // The marked guest after midnight. Its cell and HP are its Guest record's.
    struct Taken
    {
        bool present = false;
        std::int8_t guest = -1, cooldown = 0;
        bool reflect = false;
    };

    struct Monster
    {
        std::int8_t kind = -1, cell = -1, leavesRound = 0; // kind: monsters.csv row
        std::int16_t hp = 0, might = 0, sanity = 0;        // grown with the hour it appeared
    };

    // One side of a fight: a guest (or the Taken) or a monster, with its buffs (buffs.csv rows).
    struct Fighter
    {
        std::int8_t guest = -1, monster = -1; // monster: index in NightState::monsters
        bool taken = false;
        std::int8_t side = 0; // 0 = the side that started
        std::array<std::int8_t, 4> buff{-1, -1, -1, -1}, buffRounds{};
    };

    struct Battle
    {
        bool active = false;
        std::int8_t round = 1, turn = 0, count = 0; // turn: index in fighters
        std::array<Fighter, 8> fighters{};
        std::int8_t cell = -1, owner = -1; // owner: the guest whose board turn it is
    };

    // Everything that changes during a night. No pointers, strings or maps: bots copy it to look ahead.
    struct NightState
    {
        std::uint64_t seed = 0, random = 0;
        std::array<Cell, maxCells> cells{};
        std::array<std::int8_t, 64> deck{}; // rooms.csv rows still to place, shuffled
        std::int8_t deckSize = 0, deckNext = 0, placed = 0;
        std::int8_t foyer = -1, understage = -1; // cells

        std::array<Guest, maxGuests> guests{};
        std::array<std::int8_t, maxGuests> order{}; // who acts in which place of a round
        std::int8_t count = 0;                      // guests tonight
        std::int8_t round = 1, turn = 0;            // turn: place in `order`
        std::int8_t player = -1;
        bool over = false; // the night has ended
        std::vector<FloorItem> floor;
        std::array<std::int16_t, maxGuests * maxGuests> bonds{}; // from * maxGuests + to, -1000..1000
        std::int8_t marked = -1, omensTaken = 0; // marked: the guest who turns (set at a failed roll or at midnight)
        std::int8_t omenOffer = -1;          // the cell whose omen the board guest must take or leave
        std::uint8_t omensUsed = 0;          // omens.csv rows already drawn (bits)
        Outcome outcome = Outcome::Running;
        Taken taken;
        std::array<Mark, 16> flooded{}, sealed{};
        Battle battle;
        std::vector<Monster> monsters;
        std::uint64_t eventsUsed = 0, itemsUsed = 0; // events.csv / items.csv rows already drawn
        std::array<LogEntry, 256> log{};             // a ring of the last 256
        std::uint32_t logCount = 0;                  // entries ever written
    };

    // A night with every guest in the foyer; `player` is the guest a person plays (-1: bots only).
    NightState NewNight(const NightDef &def, std::uint64_t seed, int player = -1);

    // The turn (Turn.cpp).
    int Current(const NightState &state); // the guest to act; -1 when the night is over
    std::vector<Action> LegalActions(const NightDef &def, const NightState &state);
    bool Apply(const NightDef &def, NightState &state, const Action &action); // false and no change if not legal
    int Hour(const NightDef &def, const NightState &state);
    int Minute(const NightDef &def, const NightState &state);
    // The company (Company.cpp).
    int Bond(const NightState &state, int from, int to);
    bool Controls(const NightState &state, int controller, int guest); // the player's guest and their group 0

    Outcome Result(const NightState &state);
    // The curse roll's target for the next omen: targets[omens taken] + perHour * hours since the night began.
    int CurseTarget(const NightDef &def, const NightState &state);
    bool IsTaken(const NightState &state, int guest);

    // A hit in battle (Battle.cpp): data formula, resistance, armour, guard; never below minDamage.
    int Damage(const NightDef &def, const NightState &state, const Fighter &attacker, const Fighter &defender);

    // d20 + the guest's stat against the target; no rule for 1 or 20.
    Check RollCheck(const NightDef &def, NightState &state, int guest, StatKind stat, int target);
    // Damage from a guest (byGuest) or the house (-1). Before the Turn 0 HP is down, after it death.
    void Hurt(const NightDef &def, NightState &state, int guest, int amount, int byGuest);
    // Entries written since `since` (a logCount value), oldest first; at most the last 256.
    std::vector<LogEntry> NewLog(const NightState &state, std::size_t since);

    // Base + worn items + omens (+ buffs in battle). Light: how many steps the guest's light reaches.
    int Stat(const NightDef &def, const NightState &state, int guest, StatKind kind);

    // The board (Board.cpp). Directions: 0 N, 1 E, 2 S, 3 W.
    int CellAt(const NightDef &def, int x, int y); // -1 outside the grid
    int Neighbour(const NightDef &def, int cell, int direction);
    bool Passage(const NightDef &def, const NightState &state, int from, int direction); // both opened, doors face
    bool Doorway(const NightDef &def, const NightState &state, int from, int direction); // a door to an unopened cell
    int Open(const NightDef &def, NightState &state, int from, int direction);         // places the next room
    int Steps(const NightDef &def, const NightState &state, int from, int to);          // -1 if unreachable
    bool Lit(const NightDef &def, const NightState &state, int viewer, int cell, int steps);
    bool InLine(const NightDef &def, const NightState &state, int from, int to); // straight, unbroken passages
}
