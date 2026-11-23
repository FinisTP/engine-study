#pragma once

#include <MatchDef.hpp>

#include <array>
#include <cstdint>
#include <vector>

// The faction match as one copyable value and the rules that change it (Match.md).
namespace umbra::match
{
    // ---- The board (Board.cpp, Board.md) ----

    enum class CellKind : std::uint8_t { Wall, Hall, Room, Centre, Hub };

    // One square. `doors`: bit 0 = N, 1 = E, 2 = S, 3 = W. `event`: an events.csv row, -1 none.
    struct Cell
    {
        CellKind kind = CellKind::Wall;
        std::uint8_t doors = 0;
        std::int8_t event = -1;
        bool resolved = false;
    };

    // Cells are y * width + x. Side 0's hub is in the bottom row, side 1's in the top row.
    // `door`: the square between each hub and its lanes, where its guardian stands.
    struct Board
    {
        int width = 0, height = 0;
        std::vector<Cell> cells;
        std::array<std::int16_t, 2> hub{}, door{};
        std::vector<std::int16_t> stageRooms, weakDens, strongDens, itemRooms; // both halves
        std::vector<std::int16_t> centreSpawns; // middle-row squares either side of the stage
    };

    Board BuildBoard(const MatchDef &def, std::uint64_t seed);
    int Mirror(const Board &b, int cell);            // x, y -> x, height - 1 - y
    int Neighbour(const Board &b, int cell, int dir); // through a door, or -1
    int Steps(const Board &b, int from, int to);     // -1 if unreachable
    bool Fair(const Board &b);

    // ---- The match (Match.cpp, Turn.cpp, Battle.cpp, ...) ----

    // A unit's flags (Match.md "Freeing and the end"): the living player, a boss, a recruit to free,
    // still on its strings, freed, removed for good.
    namespace flag
    {
        constexpr std::uint8_t Player = 1, Boss = 2, Recruit = 4, Strung = 8, Freed = 16, Removed = 32, Spawned = 64; // Spawned: let out by a window
    }

    // A character or a monster. faction -1 = neutral. cell -1 = off the board (down, or not woken).
    // With falls (Mode::fallsStay) a fallen unit keeps its cell at 0 HP.
    struct Unit
    {
        std::int8_t kind = 0; // 0 character (characters.csv row), 1 monster (monsters.csv row)
        std::int8_t def = -1, faction = -1;
        std::int16_t cell = -1, home = -1; // home: the hub, or a monster's den
        std::int16_t hp = 0, maxHp = 0, attack = 0, defence = 0, speed = 0, move = 0, xp = 0;
        std::int8_t level = 1, size = 1; // size: squares a side; cell is the top-left one
        std::int8_t back = 0;    // turns until a downed character returns
        std::int8_t pending = -1; // 0: a level's cards wait for this unit's choice (MatchState::offer)
        int essence = 0, kills = 0;
        std::array<std::int8_t, maxAbilities> abilities{-1, -1, -1, -1, -1, -1, -1, -1};
        std::array<std::int8_t, maxItems> items{-1, -1, -1, -1};
        std::array<std::int8_t, maxAbilities> boost{}, cut{}; // level-card upgrades: +power, -HP cost, by slot
        std::uint8_t flags = 0;  // flag::
        std::int8_t ap = 0;      // action points stored by biding (Match.md "Biding")
        std::int16_t from = 0;   // the first round it may act (one that appears mid-fight waits a round)
        std::int8_t bonusMove = 0; // added to its next turn's moves, then spent (Match.md "Hasten")
        std::int8_t marked = 0, slow = 0; // more damage taken, less Speed: until its next turn (Match.md "Effects")
    };

    inline bool Up(const Unit &u) { return u.cell >= 0 && u.hp > 0; }      // on the board and standing
    inline bool Fallen(const Unit &u) { return u.cell >= 0 && u.hp <= 0; } // on the board, fallen (falls only)

    struct Floor
    {
        std::int16_t cell = -1;
        std::int8_t item = -1; // items.csv row, -1 = essence only
        int essence = 0;
        bool treasure = false; // an item room's treasure: picking it up deals face-down cards
    };

    // A unit's chance to drop an item when it falls (stage files; rolled from the fight's rng).
    struct Drop
    {
        std::int8_t unit = -1, item = -1, percent = 0;
    };

    // An item a thief took (Match.md "Effects"); it drops on the thief's square when the thief is hit.
    struct Stolen
    {
        std::int8_t thief = -1, item = -1;
        std::int16_t cell = -1; // where it is shown (a window, campaign plan 4's gimmicks); -1 with the thief
    };

    // A unit that waits off the board until `round`, then stands on `cell` (a stage's reinforcement).
    struct Arrival
    {
        std::int8_t unit = -1;
        std::int16_t cell = -1;
        std::int16_t round = 0;
    };

    // A square marked for the next round's start (Match.md "Gimmicks").
    struct Mark
    {
        enum Kind : std::int8_t { Hazard, Breath, Window };
        std::int8_t kind = Hazard;
        std::int16_t cell = -1;
    };

    // A stage's gimmicks (campaign plan 4): falling hazards, a breath that strikes a row, windows that let out spawns.
    struct Gimmicks
    {
        std::int8_t hazards = 0, hazardDamage = 0;                  // squares marked each round, and the damage when they fall
        std::int8_t breathUnit = -1, breathDamage = 0, breathEvery = 2; // marks its row every `breathEvery` rounds
        std::vector<std::int16_t> windows;                          // the window squares
        std::vector<std::vector<std::int8_t>> pattern;              // which windows open each round, cycling
        Unit spawn;                                                 // what an open window lets out
        std::int8_t spawnMax = 0;                                   // at most this many alive
        std::vector<Drop> spawnDrops;                               // its drops (unit filled in)
        std::vector<Mark> marks;                                    // for the coming round
    };

    // A three-round battle between two units (Battle.cpp).
    struct Battle
    {
        bool on = false;
        std::int8_t attacker = -1, defender = -1;               // who began it, on whom (each leads a group)
        std::int8_t round = 0, next = 0;                        // next: the place in `order` whose pick it is
        std::array<std::int8_t, 4> fighters{-1, -1, -1, -1};    // places 0-1 the attacker's group, 2-3 the defender's
        std::array<std::int8_t, 4> order{-1, -1, -1, -1};       // this round's picks, by place (Match.md "Group battles")
        std::array<std::int16_t, 4> shield{};                   // by place
    };

    // The current character's turn: where it began, steps left, whether it has acted.
    struct TurnState
    {
        std::int16_t origin = -1;
        std::int8_t movesLeft = 0;
        bool acted = false;
        std::int8_t apSpent = 0; // action points spent this turn
    };

    enum class LogKind : std::uint8_t { Moved, Battle, Blow, Down, Essence, Performed, Level, Back, SpawnSoon, Spawned, Event, Item, Treasure, Over, Healed, Bided, Freed, Arriving, Arrived, Hazard, Breath }; // Freed: a = the player, b = the recruit, c = its square; Arriving/Arrived: a = the unit, b = its square; Hazard: a = the unit hurt, b = its square, c = damage; Breath: a = the breather
    struct LogEntry
    {
        LogKind kind = LogKind::Moved;
        std::int16_t a = -1, b = -1, c = 0;
    };

    // A monster due on a spawn square (dens and centreSpawns) at the start of `round`, or once the square is free.
    struct Spawn
    {
        std::int16_t cell = -1;
        std::int8_t monster = -1;
        int round = 0;
    };

    // A card a level offers (Match.md "Level cards"). Learn: a = ability. Stat: a = Stat, b = amount.
    // Upgrade: a = the ability's slot, b = 0 power, 1 cost.
    struct LevelCard
    {
        std::int8_t kind = 0, a = -1, b = 0;
        bool operator==(const LevelCard &) const = default;
    };

    // A claimed room coming back (Match.md "Rooms come back"): kind 0 an event, 1 a treasure.
    struct RoomSpawn
    {
        std::int16_t cell = -1;
        std::int8_t kind = 0;
        int round = 0;
    };

    // Why a campaign fight was lost (Match.md "Freeing and the end").
    enum class Loss : std::uint8_t { None, PlayerFell, SideDown, Trapped };

    struct MatchState
    {
        Mode mode; // the switches (Match.md "Modes")
        Board board;
        std::vector<Unit> units; // side 0's characters, side 1's, then monsters
        std::vector<std::int8_t> order; // every unit by Speed, sorted again at each round's start (Match.md "The order")
        std::uint64_t orderSeed = 0;    // ties between equal Speeds
        int turn = 0, round = 1;
        std::array<int, 2> score{};
        std::int16_t stage = -1; // the centre of the middle row, fixed
        std::vector<Spawn> spawns; // shown once round - spawn.warn has come
        std::vector<Floor> floor;
        std::vector<RoomSpawn> roomSpawns;
        std::vector<std::int8_t> cards;
        std::vector<LevelCard> offer; // the level cards the pending unit chooses from // treasure.csv rows dealt face down; the acting character must pick one
        Battle battle;
        TurnState now;
        std::vector<LogEntry> log;
        std::uint64_t rng = 0;
        bool over = false;
        int winner = -1; // -1 a draw
        Loss loss = Loss::None;
        bool riteReady = false, riteUsed = false; // Le Thanh's rite: offered once in the whole game (plan 5 saves it)
        std::vector<std::int8_t> found; // ingredients and keepsakes picked up or dropped: for the cafe's storage (Match.md "Found")
        std::vector<Drop> drops;        // what each unit may drop when it falls
        std::vector<Arrival> arrivals;  // reinforcements still to come
        std::uint8_t noRevive = 0;      // sides (bit 1 << side) that never revive
        std::vector<Stolen> stash;      // what thieves hold
        Gimmicks gimmicks;
    };

    // A unit's squares: size x size from its cell, the top-left (Match.md "Large monsters"), fallen or not.
    // Covers: whether it stands on a square (a fallen unit covers none, so it doesn't block).
    std::vector<int> Squares(const Board &b, const Unit &u);
    bool Covers(const Board &b, const Unit &u, int cell);

    // A unit from a characters.csv row (home: its hub) or a monsters.csv row (standing on `cell`).
    Unit FromCharacter(const MatchDef &def, int row, int faction, int home);
    Unit FromMonster(const MatchDef &def, int row, int faction, int cell);

    // Starts a fight whose units stand placed: everyone may act, the order by Speed (ties from `seed`),
    // round 1, the first turn (monsters faster than everyone act at once). NewMatch ends with it.
    void BeginMatch(const MatchDef &def, MatchState &s, std::uint64_t seed);

    // picks: side 0's characters.csv rows (options.side of them, no repeats). Side 1 is RiggedPick.
    MatchState NewMatch(const MatchDef &def, std::uint64_t seed, const std::vector<int> &picks);
    // No role twice; Defender, Supporter, All-Rounder and Attacker first, then the rest.
    std::vector<int> RiggedPick(const MatchDef &def, std::uint64_t seed, int count);
    int TurnsLeft(const MatchDef &def, const MatchState &s); // rounds left, -1 with no round limit

    // ---- Actions (Turn.cpp) ----

    // Bide: the turn passes for one action point. Extra: spend one after acting for another move and action
    // (Match.md "Biding").
    // Cut: the player frees a fallen strung recruit beside it (a = unit). Rite: after the player fell, Le
    // Thanh's rite stands the side up, once a game (Match.md "Freeing and the end").
    enum class ActionKind : std::uint8_t { Move, Undo, Attack, BattleAbility, BoardAbility, UseItem, PickUp, Perform, Answer, LevelUp, EndTurn, Card, Bide, Extra, Cut, Rite };

    // Move: a = direction 0-3. Attack: a = unit. BattleAbility: a = ability slot. BoardAbility: a = slot,
    // b = target unit. UseItem: a = item slot, b = the target of a revive item (else -1). PickUp: a = floor index. Answer: a = choice. LevelUp: a = 0 or 1.
    // Card: a = the dealt card.
    struct Action
    {
        ActionKind kind = ActionKind::EndTurn;
        std::int8_t a = -1, b = -1;
        bool operator==(const Action &) const = default;
    };

    // Why an action cannot be taken; the app shows it as a tooltip (the user's design rule).
    enum class Why : std::uint8_t
    {
        Ok, NotYourTurn, NoMovesLeft, Blocked, AlreadyActed, OutOfRange, OwnSide, TooCostly,
        NotOnStage, NothingToPerform, NothingHere, BagFull, MustChoose, Over, FullHp,
        Off,      // not in this fight's mode (Match.md "Modes")
        Fallen,   // the target has fallen (Match.md "Falls")
        NotFallen, // revive and remove need a fallen target
        NoAp,      // not enough action points, or none left to spend this turn
        OnlyPlayer, // only the living player cuts strings
        NotStrung, // no gold knot to cut
        NoRite,    // the rite is not offered now
        NotHere    // the item can't be used here (items.csv "use")
    };

    int Current(const MatchState &s); // the unit that acts next; -1 when the match is over (the player while the rite is offered)
    Why Check(const MatchDef &def, const MatchState &s, const Action &a);
    std::vector<Action> Legal(const MatchDef &def, const MatchState &s);
    bool Apply(const MatchDef &def, MatchState &s, const Action &a); // false and no change unless Check is Ok

    // ---- Battles (Battle.cpp) ----

    // What an action would do, shown before it is taken (dice give a range).
    struct Forecast
    {
        int dealtMin = 0, dealtMax = 0, healed = 0, cost = 0;
    };
    Forecast Preview(const MatchDef &def, const MatchState &s, const Action &a);
    // power (or the roll, for dice) + Attack - (Defence + battle shield), at least battle.minDamage.
    int Damage(const MatchDef &def, const MatchState &s, int ability, int from, int to, int roll);

    // A monster's turn: it acts by its behaviour (Monsters.cpp). The order calls it at the monster's place.
    void MonsterAct(const MatchDef &def, MatchState &s, int unit);
    // The battle ability slot a monster picks now (MonsterMind.cpp).
    int MonsterPick(const MatchDef &def, const MatchState &s, int unit);
    int MonsterTarget(const MatchDef &def, const MatchState &s, int unit); // the foe it strikes: one it can down, else the weakest
}
