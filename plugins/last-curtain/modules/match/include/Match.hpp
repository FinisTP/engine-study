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

    // A character or a monster. faction -1 = neutral. cell -1 = off the board (down, or not woken).
    struct Unit
    {
        std::int8_t kind = 0; // 0 character (characters.csv row), 1 monster (monsters.csv row)
        std::int8_t def = -1, faction = -1;
        std::int16_t cell = -1, home = -1; // home: the hub, or a monster's den
        std::int16_t hp = 0, maxHp = 0, attack = 0, defence = 0, speed = 0, move = 0, xp = 0;
        std::int8_t level = 1, size = 1; // size: squares a side; cell is the top-left one
        std::int8_t back = 0;    // turns until a downed character returns
        std::int8_t pending = -1; // a jobs.csv row waiting for its choice
        int essence = 0, kills = 0;
        std::array<std::int8_t, maxAbilities> abilities{-1, -1, -1, -1, -1, -1, -1, -1};
        std::array<std::int8_t, maxItems> items{-1, -1, -1, -1};
    };

    struct Floor
    {
        std::int16_t cell = -1;
        std::int8_t item = -1; // items.csv row, -1 = essence only
        int essence = 0;
        bool treasure = false; // an item room's treasure: picking it up deals face-down cards
    };

    // A three-round battle between two units (Battle.cpp).
    struct Battle
    {
        bool on = false;
        std::int8_t attacker = -1, defender = -1;
        std::int8_t round = 0, next = 0; // next to pick: 0 attacker, 1 defender
        std::int8_t first = 0;           // who picks first each round
        std::array<std::int16_t, 2> shield{};
    };

    // The current character's turn: where it began, steps left, whether it has acted.
    struct TurnState
    {
        std::int16_t origin = -1;
        std::int8_t movesLeft = 0;
        bool acted = false;
    };

    enum class LogKind : std::uint8_t { Moved, Battle, Blow, Down, Essence, Performed, Level, Back, SpawnSoon, Spawned, Event, Item, Treasure, Over };
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

    // A claimed room coming back (Match.md "Rooms come back"): kind 0 an event, 1 a treasure.
    struct RoomSpawn
    {
        std::int16_t cell = -1;
        std::int8_t kind = 0;
        int round = 0;
    };

    struct MatchState
    {
        Board board;
        std::vector<Unit> units; // side 0's characters, side 1's, then monsters
        std::vector<std::int8_t> order; // characters by Speed, set once
        int turn = 0, round = 1;
        std::array<int, 2> score{};
        std::int16_t stage = -1; // the centre of the middle row, fixed
        int monsterTurn = -1;    // the monster acting in the monsters' phase, -1 outside it
        std::vector<Spawn> spawns; // shown once round - spawn.warn has come
        std::vector<Floor> floor;
        std::vector<RoomSpawn> roomSpawns;
        std::vector<std::int8_t> cards; // treasure.csv rows dealt face down; the acting character must pick one
        Battle battle;
        TurnState now;
        std::vector<LogEntry> log;
        std::uint64_t rng = 0;
        bool over = false;
        int winner = -1; // -1 a draw
    };

    // A unit's squares: size x size from its cell, the top-left (Match.md "Large monsters").
    std::vector<int> Squares(const Board &b, const Unit &u);
    bool Covers(const Board &b, const Unit &u, int cell);

    // picks: side 0's characters.csv rows (options.side of them, no repeats). Side 1 is RiggedPick.
    MatchState NewMatch(const MatchDef &def, std::uint64_t seed, const std::vector<int> &picks);
    // No role twice; Defender, Supporter, All-Rounder and Attacker first, then the rest.
    std::vector<int> RiggedPick(const MatchDef &def, std::uint64_t seed, int count);
    int TurnsLeft(const MatchDef &def, const MatchState &s);

    // ---- Actions (Turn.cpp) ----

    enum class ActionKind : std::uint8_t { Move, Undo, Attack, BattleAbility, BoardAbility, UseItem, PickUp, Perform, Answer, LevelUp, EndTurn, Card };

    // Move: a = direction 0-3. Attack: a = unit. BattleAbility: a = ability slot. BoardAbility: a = slot,
    // b = target unit. UseItem: a = item slot. PickUp: a = floor index. Answer: a = choice. LevelUp: a = 0 or 1.
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
        NotOnStage, NothingToPerform, NothingHere, BagFull, MustChoose, Over
    };

    int Current(const MatchState &s); // the unit that acts next, -1 when the match is over
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

    // Monsters act once a round, after the last character, one by one (Monsters.cpp).
    void RunMonsters(const MatchDef &def, MatchState &s);
    // The battle ability slot a monster picks now (MonsterMind.cpp).
    int MonsterPick(const MatchDef &def, const MatchState &s, int unit);
}
