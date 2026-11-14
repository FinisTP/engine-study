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

    void BuildBoard(const NightDef &def, NightState &state); // Board.cpp: deck and foyer
}
