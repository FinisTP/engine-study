#include "NightTestData.hpp"

#include <BoardView.hpp>

#include <algorithm>

using namespace umbra::night;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }
    std::int8_t I8(int i) { return static_cast<std::int8_t>(i); }

    // A corridor along row 4 to the left of the foyer, every guest in the foyer, guest 0 the player.
    NightState Corridor(const NightDef &def)
    {
        NightState state = NewNight(def, 1, 0);
        const int room = NightDef::Find(def.rooms, "auditorium");
        for (int x = 0; x < 2; ++x)
            state.cells[U(CellAt(def, x, 4))] = {I8(room), 2 | 8, true}; // E and W doors
        state.cells[U(state.foyer)].doors |= 8;
        return state;
    }

    bool Shows(const BoardView &view, int guest)
    {
        return std::any_of(view.pawns.begin(), view.pawns.end(), [&](const PawnView &p) { return p.guest == guest; });
    }
}

TEST_CASE("Only lit rooms show who is in them (T108)", "[night][board]")
{
    NightDef def = Data();
    def.rules.light.steps = 1;
    NightState state = Corridor(def);
    state.guests[1].cell = I8(CellAt(def, 1, 4)); // one step from the player: lit
    state.guests[2].cell = I8(CellAt(def, 0, 4)); // two steps: dark
    const BoardView view = MakeView(def, state, 0);
    CHECK(Shows(view, 0));
    CHECK(Shows(view, 1));
    CHECK_FALSE(Shows(view, 2));
    CHECK(view.cells[U(CellAt(def, 1, 4))].lit);
    CHECK_FALSE(view.cells[U(CellAt(def, 0, 4))].lit);
    CHECK(view.cells[U(CellAt(def, 0, 4))].opened); // its shape is known
}

TEST_CASE("Allies' light counts (T108)", "[night][board]")
{
    NightDef def = Data();
    def.rules.light.steps = 1;
    NightState state = Corridor(def);
    state.guests[1].cell = I8(CellAt(def, 1, 4));
    state.guests[2].cell = I8(CellAt(def, 0, 4));
    state.guests[0].group = state.guests[1].group = 0;
    const BoardView view = MakeView(def, state, 0);
    CHECK(view.cells[U(CellAt(def, 0, 4))].lit);
    CHECK(Shows(view, 2));
    const auto ally = std::find_if(view.pawns.begin(), view.pawns.end(), [](const PawnView &p) { return p.guest == 1; });
    REQUIRE(ally != view.pawns.end());
    CHECK(ally->ally);
}

TEST_CASE("Minions and the Taken show only in light (T108)", "[night][board]")
{
    NightDef def = Data();
    def.rules.light.steps = 1;
    NightState state = Corridor(def);
    state.taken = {true, 3, 0, false};
    state.guests[3].cell = I8(CellAt(def, 0, 4));
    state.monsters.push_back({0, I8(CellAt(def, 0, 4)), 99, 5, 1, 1});
    state.monsters.push_back({0, I8(CellAt(def, 1, 4)), 99, 5, 1, 1});
    BoardView view = MakeView(def, state, 0);
    CHECK_FALSE(Shows(view, 3));
    REQUIRE(view.monsters.size() == 1);
    CHECK(view.monsters[0].monster == 1);
    state.guests[3].cell = I8(CellAt(def, 1, 4));
    view = MakeView(def, state, 0);
    const auto taken = std::find_if(view.pawns.begin(), view.pawns.end(), [](const PawnView &p) { return p.guest == 3; });
    REQUIRE(taken != view.pawns.end());
    CHECK(taken->taken);
}

TEST_CASE("Steps are the legal moves of the viewer's turn (T108)", "[night][board]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 1, 0);
    while (Current(state) != 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    BoardView view = MakeView(def, state, 0);
    const auto legal = LegalActions(def, state);
    int steps = 0;
    for (int c = 0; c < maxCells; ++c)
        if (view.cells[U(c)].step >= 0)
        {
            const Action &a = legal[U(view.cells[U(c)].step)];
            REQUIRE(a.kind == ActionKind::Move);
            CHECK(Neighbour(def, state.foyer, a.a) == c);
            ++steps;
        }
    CHECK(steps > 0);
    REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    view = MakeView(def, state, 0);
    CHECK(std::none_of(view.cells.begin(), view.cells.end(), [](const CellView &c) { return c.step >= 0; }));
}

TEST_CASE("Cells and points agree (T108)", "[night][board]")
{
    const NightDef def = Data();
    for (int c = 0; c < def.rules.board.width * def.rules.board.height; ++c)
        CHECK(CellAtPoint(def, CellCenter(def, c)) == c);
    CHECK(CellAtPoint(def, {100.0F, 0.0F, 0.0F}) == -1);
    CHECK(CellCenter(def, CellAt(def, 2, 2)).x == 0.0F); // the board is centred
}

TEST_CASE("A dead player sees everything (T108)", "[night][board]")
{
    NightDef def = Data();
    def.rules.light.steps = 0;
    NightState state = Corridor(def);
    state.guests[2].cell = I8(CellAt(def, 0, 4));
    state.guests[0].dead = true;
    CHECK(Shows(MakeView(def, state, -1), 2));
    CHECK(Shows(MakeView(def, state, 0), 2)); // a dead viewer watches with everyone's eyes
}
