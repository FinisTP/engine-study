#pragma once

#include <NightDef.hpp>

#include <array>
#include <cstdint>

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
    };

    // Everything that changes during a night. No pointers, strings or maps: bots copy it to look ahead.
    struct NightState
    {
        std::uint64_t seed = 0, random = 0;
        std::array<Cell, maxCells> cells{};
        std::array<std::int8_t, 64> deck{}; // rooms.csv rows still to place, shuffled
        std::int8_t deckSize = 0, deckNext = 0, placed = 0;
        std::int8_t foyer = -1, understage = -1; // cells
    };

    NightState NewNight(const NightDef &def, std::uint64_t seed);

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
