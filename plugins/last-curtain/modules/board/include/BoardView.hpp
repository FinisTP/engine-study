#pragma once

#include <Night.hpp>

#include <glm/vec3.hpp>

#include <array>
#include <vector>

// The night as the player may see it (Board.md): what their light and their allies' light
// reaches, and where things stand on the table in metres. No renderer here: it is tested headless.
namespace umbra::night
{
    struct CellView
    {
        bool opened = false, lit = false;
        RoomKind kind = RoomKind::Plain;
        std::uint8_t doors = 0;    // NESW, bit 0 = N
        std::uint8_t doorways = 0; // doors that lead to a room not yet opened
        std::uint8_t walls = 0;    // sides a guest cannot pass: neither a passage nor a doorway
        std::int8_t step = -1;     // index in LegalActions of the Move into this cell, on the viewer's turn
    };

    struct PawnView
    {
        int guest = -1, cell = -1;
        bool player = false, ally = false, taken = false;
    };

    struct MonsterView
    {
        int monster = -1, cell = -1;
        bool minion = false;
    };

    struct BoardView
    {
        std::array<CellView, maxCells> cells{};
        std::vector<PawnView> pawns;
        std::vector<MonsterView> monsters;
    };

    // viewer: the player's guest; -1 (or a dead viewer) sees everything.
    BoardView MakeView(const NightDef &def, const NightState &state, int viewer);

    inline constexpr float cellSize = 2.4F; // metres from one cell's centre to the next

    // The centre of a cell on the table (y = 0), the board centred on the origin, row 0 at -z.
    glm::vec3 CellCenter(const NightDef &def, int cell);
    // The cell under a point of the table, -1 outside the board.
    int CellAtPoint(const NightDef &def, glm::vec3 point);
    // The cell a ray (from the camera through the cursor) meets on the table top (y = 0), -1 if none.
    int CellUnderRay(const NightDef &def, glm::vec3 origin, glm::vec3 direction);
}
