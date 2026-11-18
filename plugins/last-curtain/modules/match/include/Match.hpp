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
    };

    Board BuildBoard(const MatchDef &def, std::uint64_t seed);
    int Mirror(const Board &b, int cell);            // x, y -> x, height - 1 - y
    int Neighbour(const Board &b, int cell, int dir); // through a door, or -1
    int Steps(const Board &b, int from, int to);     // -1 if unreachable
    bool Fair(const Board &b);
}
