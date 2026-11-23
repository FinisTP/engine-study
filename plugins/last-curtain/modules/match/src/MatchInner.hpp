#pragma once

#include <cstddef>
#include <cstdint>

// Small helpers shared by the match's source files.
namespace umbra::match::inner
{
    inline std::size_t U(int i) { return static_cast<std::size_t>(i); }

    // splitmix64: everything random follows from one 64-bit state.
    struct Rng
    {
        std::uint64_t s = 0;
        std::uint64_t Next()
        {
            std::uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        }
        int Below(int n) { return n <= 0 ? 0 : static_cast<int>(Next() % static_cast<std::uint64_t>(n)); }
    };
}

#include <Match.hpp>

namespace umbra::match::inner
{
    // Turn.cpp
    int UnitAt(const MatchState &s, int cell); // a unit on the square, or -1
    bool GuardianStands(const MatchDef &def, const MatchState &s, int faction);
    void Log(MatchState &s, LogKind kind, int a = -1, int b = -1, int c = 0);
    int Roll(MatchState &s, int n); // 0 .. n-1 from the match's random numbers
    void Acted(const MatchDef &def, MatchState &s); // the turn's action is taken
    void NextTurn(const MatchDef &def, MatchState &s);
    void SortOrder(MatchState &s); // every unit by Speed, ties by a key from orderSeed
    bool OnTurn(const MatchState &s); // a character's own board turn (no battle, no level choice)
}

namespace umbra::match::inner
{
    // Battle.cpp
    Why CheckAttack(const MatchDef &def, const MatchState &s, int me, int target);
    Why CheckBattleAbility(const MatchDef &def, const MatchState &s, int me, int slot, int target);
    int Place(const MatchState &s, int unit);     // its place among the battle's fighters, -1 none (0-1 attackers, 2-3 defenders)
    bool Standing(const MatchState &s, int place); // that place's fighter is still in the fight
    int Picker(const MatchState &s);              // whose pick it is
    int FirstFoe(const MatchState &s, int me);     // the first standing fighter of the other group
    void StartBattle(const MatchDef &def, MatchState &s, int me, int target);
    void Pick(const MatchDef &def, MatchState &s, int me, int slot, int target); // target -1: the first foe standing
    void Down(const MatchDef &def, MatchState &s, int victim, int killer);
    bool Adjacent(const Board &b, int a, int c); // one step apart through a door
    bool Beside(const Board &b, const Unit &a, const Unit &c); // any square of one next to any of the other
    int StepsTo(const Board &b, int from, const Unit &t); // to its nearest square, -1 if unreachable
    // Room on a square for one more of `faction`: under squareCapacity, no foe there, the stage only ever one.
    bool Room(const MatchDef &def, const MatchState &s, int cell, int faction);
    int StraightTo(const Board &b, int from, const Unit &t); // the same counted straight, walls ignored
}

namespace umbra::match::inner
{
    // Abilities.cpp
    void GainXp(const MatchDef &def, MatchState &s, int unit, int amount);
    int PowerOf(const MatchDef &def, const Unit &u, int slot); // the ability's power with this unit's upgrades
    int CostOf(const MatchDef &def, const Unit &u, int slot);  // ... its HP cost, never below 0
    Why CheckLevelUp(const MatchDef &def, const MatchState &s, int me, int choice);
    Why CheckBoardAbility(const MatchDef &def, const MatchState &s, int me, int slot, int target);
    Why CheckUseItem(const MatchDef &def, const MatchState &s, int me, int slot);
    Why CheckAnswer(const MatchDef &def, const MatchState &s, int me, int choice);
    void LevelUp(const MatchDef &def, MatchState &s, int me, int choice);
    void BoardAbility(const MatchDef &def, MatchState &s, int me, int slot, int target);
    void UseItem(const MatchDef &def, MatchState &s, int me, int slot);
    void Answer(const MatchDef &def, MatchState &s, int me, int choice);
}

namespace umbra::match::inner
{
    // Treasure.cpp (Match.md "Treasure")
    void DealCards(const MatchDef &def, MatchState &s);
    void TakeCard(const MatchDef &def, MatchState &s, int me, int card);
    void RoomBack(const MatchDef &def, MatchState &s, int claimed, int kind); // a claimed room comes back later
    void RunRoomSpawns(const MatchDef &def, MatchState &s);                 // at the start of each round
}

namespace umbra::match::inner
{
    // Stage.cpp
    Why CheckPerform(const MatchDef &def, const MatchState &s, int me);
    void Perform(const MatchDef &def, MatchState &s, int me);
}

namespace umbra::match::inner
{
    // Monsters.cpp (Match.md "Spawns")
    int ZoneOf(const Board &b, int cell);                         // 0 weak den, 1 strong den, 2 centre, -1 none
    int SpawnKind(const MatchDef &def, int zone, int round);     // the zone's latest row by that round, or -1
    void ScheduleSpawn(const MatchDef &def, MatchState &s, int cell); // its monster was downed
    void RunSpawns(const MatchDef &def, MatchState &s, bool roundStart);
    // Every square of a size x size unit at `anchor` is on the board, open, not the stage, free of others.
    bool Fits(const MatchState &s, int size, int anchor, int self = -1);
}
