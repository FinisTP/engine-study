#pragma once

#include <Night.hpp>

// Helpers shared by the rule files.
namespace umbra::night::inner
{
    // xorshift64*: three shifts and a multiply, the same on every compiler.
    inline std::uint64_t Dice(NightState &state)
    {
        state.random ^= state.random >> 12;
        state.random ^= state.random << 25;
        state.random ^= state.random >> 27;
        return state.random * 2685821657736338717ULL;
    }
    inline int Below(NightState &state, int count) { return static_cast<int>((Dice(state) >> 33) % static_cast<std::uint64_t>(count)); }
    inline int Roll(NightState &state, int sides) { return 1 + Below(state, sides); }

    inline const RoomDef &RoomOf(const NightDef &def, const NightState &state, int cell)
    {
        return def.rooms[static_cast<std::size_t>(state.cells[static_cast<std::size_t>(cell)].room)];
    }
    inline std::size_t U(int i) { return static_cast<std::size_t>(i); }
    template <class T> std::int8_t I8(T v) { return static_cast<std::int8_t>(v); }
    template <class T> std::int16_t I16(T v) { return static_cast<std::int16_t>(v); }

    inline void Log(NightState &state, LogKind kind, int who, int other = -1, int a = 0, int b = 0)
    {
        state.log[state.logCount++ % state.log.size()] = {I16(state.round * 8 + state.turn), kind, I8(who), I8(other), I16(a), I16(b)};
    }

    // Board.cpp: the deck, the foyer and one start room per guest (returned, in order of placing).
    std::vector<int> BuildBoard(const NightDef &def, NightState &state);
    std::vector<int> Spawns(const NightDef &def, NightState &state); // start rooms, as far apart as the map allows
    void Dawn(const NightDef &def, NightState &state);                // a new night for the living (Night.cpp)
    void KillerRound(const NightDef &def, NightState &state);         // wakes or moves the killer (Killer.cpp)
    bool IntoKiller(const NightDef &def, NightState &state, int guest); // a guest walked into the killer: dead
    int OpenRoom(const NightDef &def, NightState &state, int from, int direction, bool forceUnderStage, bool allowUnderStage = true);
    void ForceUnderStage(const NightDef &def, NightState &state); // at the Turn, if nobody found it

    // Turn order: high roll first, then higher speed, then lower index (Turn.cpp).
    std::array<std::int8_t, maxGuests> Order(const std::array<int, maxGuests> &rolls, const std::array<int, maxGuests> &speeds, int count);
    int BoardGuest(const NightState &state); // whose board turn it is, ignoring battles and levels
    int MonsterAt(const NightState &state, int cell); // a living monster there, or -1
    // Battle.cpp
    void StartBattle(const NightDef &def, NightState &state, int cell, const std::vector<Fighter> &starters, const std::vector<Fighter> &others);
    std::vector<Fighter> Side(const NightState &state, int guest); // a guest and their allies in the room
    // Company.cpp
    void CompanyLegal(const NightDef &def, const NightState &state, int guest, std::vector<Action> &legal);
    void OpenChat(const NightDef &def, NightState &state, int opener, int other, bool askedByOpener); // draws a question
    void AnswerChat(const NightDef &def, NightState &state, int answer);
    void EndNight(NightState &state, Outcome outcome);
    // Hunt.cpp
    void PlaceKeepsake(const NightDef &def, NightState &state, int cell); // a just-opened room's keepsake, if a drawn goal needs it
    bool ContactBattle(const NightDef &def, NightState &state, int mover, int cell); // the Taken meets the company
    bool CompanyAlive(const NightState &state);
    void PowerLegal(const NightDef &def, const NightState &state, std::vector<Action> &legal);
    void UsePower(const NightDef &def, NightState &state, const Action &action);
    bool Flooded(const NightState &state, int cell);
    void TakenTurnStart(NightState &state);
    void FloodEndTurn(const NightDef &def, NightState &state, int guest); // damage for ending a turn in the water
    void BattleLegal(const NightDef &def, const NightState &state, std::vector<Action> &legal);
    void BattleApply(const NightDef &def, NightState &state, const Action &action);
    void RoundStart(const NightDef &def, NightState &state); // monsters leave and appear
    int BuffBonus(const NightDef &def, const NightState &state, int guest, StatKind kind); // battle buffs on a guest
    void HealGuest(const NightDef &def, NightState &state, int guest, int amount);
    void StartTurn(const NightDef &def, NightState &state); // the current guest's moves
    void EndTurn(const NightDef &def, NightState &state);   // bonus room, then the next guest
}
