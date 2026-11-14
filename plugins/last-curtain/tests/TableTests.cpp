#include "NightTestData.hpp"

#include <BoardView.hpp>

using namespace umbra::night;

TEST_CASE("Picking hits the cell under the cursor (T110)", "[night][view]")
{
    const NightDef def = Data();
    const int cells = def.rules.board.width * def.rules.board.height;
    for (int c = 0; c < cells; ++c)
        CHECK(CellUnderRay(def, CellCenter(def, c) + glm::vec3{0.3F, 5.0F, -0.2F}, {0.0F, -1.0F, 0.0F}) == c);
    CHECK(CellUnderRay(def, {0.0F, 2.0F, 0.0F}, {1.0F, 0.0F, 0.0F}) == -1); // parallel to the board
    CHECK(CellUnderRay(def, {0.0F, 2.0F, 0.0F}, {0.0F, 1.0F, 0.0F}) == -1); // away from it
    CHECK(CellUnderRay(def, {100.0F, 5.0F, 0.0F}, {0.0F, -1.0F, 0.0F}) == -1); // off the board
}
