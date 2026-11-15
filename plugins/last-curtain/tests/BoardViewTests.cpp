#include "NightTestData.hpp"

#include <BoardView.hpp>

#include <algorithm>

using namespace umbra::night;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }
    std::int8_t I8(int i) { return static_cast<std::int8_t>(i); }

    // The cell `n` steps left of the foyer, on its row.
    int Left(const NightDef &def, int n) { return CellAt(def, def.rules.board.width / 2 - n, def.rules.board.height - 1); }

    // A two-room corridor to the left of the foyer, every guest in the foyer, guest 0 the player.
    NightState Corridor(const NightDef &def)
    {
        NightState state = FoyerNight(def, 1, 0);
        for (int g = 0; g < state.count; ++g)
            state.guests[U(g)].cell = state.foyer;
        const int room = NightDef::Find(def.rooms, "auditorium");
        for (int n = 1; n <= 2; ++n)
            state.cells[U(Left(def, n))] = {I8(room), 2 | 8, true}; // E and W doors
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
    state.guests[1].cell = I8(Left(def, 1)); // one step from the player: lit
    state.guests[2].cell = I8(Left(def, 2)); // two steps: dark
    const BoardView view = MakeView(def, state, 0);
    CHECK(Shows(view, 0));
    CHECK(Shows(view, 1));
    CHECK_FALSE(Shows(view, 2));
    CHECK(view.cells[U(Left(def, 1))].lit);
    CHECK_FALSE(view.cells[U(Left(def, 2))].lit);
    CHECK(view.cells[U(Left(def, 2))].opened); // its shape is known
}

TEST_CASE("Allies' light counts (T108)", "[night][board]")
{
    NightDef def = Data();
    def.rules.light.steps = 1;
    NightState state = Corridor(def);
    state.guests[1].cell = I8(Left(def, 1));
    state.guests[2].cell = I8(Left(def, 2));
    state.guests[0].group = state.guests[1].group = 0;
    const BoardView view = MakeView(def, state, 0);
    CHECK(view.cells[U(Left(def, 2))].lit);
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
    state.guests[3].cell = I8(Left(def, 2));
    state.monsters.push_back({0, I8(Left(def, 2)), 99, 5, 1, 1});
    state.monsters.push_back({0, I8(Left(def, 1)), 99, 5, 1, 1});
    BoardView view = MakeView(def, state, 0);
    CHECK(Shows(view, 3)); // the Taken: everyone knows where it is after midnight (T167)
    REQUIRE(view.monsters.size() == 1);
    CHECK(view.monsters[0].monster == 1);
    state.guests[3].cell = I8(Left(def, 1));
    view = MakeView(def, state, 0);
    const auto taken = std::find_if(view.pawns.begin(), view.pawns.end(), [](const PawnView &p) { return p.guest == 3; });
    REQUIRE(taken != view.pawns.end());
    CHECK(taken->taken);
}

TEST_CASE("Steps are the legal moves of the viewer's turn (T108)", "[night][board]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 1, 0);
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
    CHECK(CellCenter(def, CellAt(def, def.rules.board.width / 2, def.rules.board.height / 2)).x == 0.0F); // the board is centred
}

TEST_CASE("A dead player sees everything (T108)", "[night][board]")
{
    NightDef def = Data();
    def.rules.light.steps = 0;
    NightState state = Corridor(def);
    state.guests[2].cell = I8(Left(def, 2));
    state.guests[0].dead = true;
    CHECK(Shows(MakeView(def, state, -1), 2));
    CHECK(Shows(MakeView(def, state, 0), 2)); // a dead viewer watches with everyone's eyes
}

TEST_CASE("A door facing a wall is drawn as a wall on both sides (T123)", "[night][view]")
{
    const NightDef def = Data();
    NightState state = Corridor(def); // foyer <-> Left(1) <-> Left(2), E and W doors
    const int a = Left(def, 1), b = Left(def, 2);
    state.cells[U(b)].doors = 8; // b keeps only its west door: a's west door faces b's wall
    BoardView view = MakeView(def, state, -1);
    CHECK((view.cells[U(a)].walls & (1u << 3)) != 0);
    CHECK((view.cells[U(b)].walls & (1u << 1)) != 0);
    CHECK((view.cells[U(a)].walls & (1u << 1)) == 0); // a <-> foyer is a passage
    CHECK((view.cells[U(state.foyer)].walls & (1u << 3)) == 0);
    // A doorway (a door to a cell not yet opened) is no wall.
    REQUIRE((view.cells[U(b)].doorways & (1u << 3)) != 0);
    CHECK((view.cells[U(b)].walls & (1u << 3)) == 0);
    // A sealed passage is a wall from both sides.
    state.sealed[0] = {I8(state.foyer), 3, 99};
    view = MakeView(def, state, -1);
    CHECK((view.cells[U(state.foyer)].walls & (1u << 3)) != 0);
    CHECK((view.cells[U(a)].walls & (1u << 1)) != 0);
}

TEST_CASE("A path walks cardinal steps through passages (T124)", "[night][view]")
{
    const NightDef def = Data();
    NightState state = Corridor(def);
    const int a = Left(def, 1), b = Left(def, 2);
    CHECK(PathTo(def, state, 0, b) == std::vector<int>{a, b});
    CHECK(PathTo(def, state, 0, a) == std::vector<int>{a});
}

TEST_CASE("A path may end through a doorway (T124)", "[night][view]")
{
    const NightDef def = Data();
    NightState state = Corridor(def);
    const int a = Left(def, 1), b = Left(def, 2), beyond = Left(def, 3);
    REQUIRE(Doorway(def, state, b, 3));
    CHECK(PathTo(def, state, 0, beyond) == std::vector<int>{a, b, beyond});
}

TEST_CASE("No path to a dark, walled-off or own cell (T124)", "[night][view]")
{
    const NightDef def = Data();
    NightState state = Corridor(def);
    Closet(def, state, 0); // cell 0: opened, no doors
    CHECK(PathTo(def, state, 0, 0).empty());
    CHECK(PathTo(def, state, 0, state.foyer).empty());
    CHECK(PathTo(def, state, 0, -1).empty());
    CHECK(PathTo(def, state, 0, maxCells).empty());
    // A room beyond the guest's light is not a lit square, even with a way there.
    for (std::uint64_t seed = 1;; ++seed)
    {
        NightState chain = FoyerNight(def, seed);
        const std::vector<int> cells = Chain(def, chain, chain.foyer, 7);
        if (cells.size() < 7)
            continue;
        const int g = Current(chain);
        REQUIRE_FALSE(Lit(def, chain, chain.foyer, cells.back(), Stat(def, chain, g, StatKind::Light)));
        CHECK(PathTo(def, chain, g, cells.back()).empty());
        break;
    }
}

TEST_CASE("A cell's view names its room, for its picture (plan 4a)", "[night][board]")
{
    const NightDef def = Data();
    const NightState state = NewNight(def, 3, 0);
    const BoardView view = MakeView(def, state, 0);
    for (int c = 0; c < maxCells; ++c)
    {
        const CellView &cell = view.cells[static_cast<std::size_t>(c)];
        if (cell.opened)
            CHECK(cell.room == state.cells[static_cast<std::size_t>(c)].room);
        else
            CHECK(cell.room == -1);
    }
    CHECK(view.cells[static_cast<std::size_t>(state.foyer)].room == NightDef::Find(def.rooms, "foyer"));
}

TEST_CASE("The view lists items lying in lit cells and each monster's kind (T142)", "[night][board]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 3, 0);
    const int here = state.guests[0].cell;
    state.floor.push_back({static_cast<std::int8_t>(here), static_cast<std::int8_t>(NightDef::Find(def.items, "lantern"))});
    state.floor.push_back({static_cast<std::int8_t>(state.foyer == here ? -1 : state.foyer), static_cast<std::int8_t>(NightDef::Find(def.items, "fan"))});
    Monster moths;
    moths.kind = static_cast<std::int8_t>(NightDef::Find(def.monsters, "moths"));
    moths.cell = static_cast<std::int8_t>(here);
    moths.hp = 3;
    state.monsters.push_back(moths);
    const BoardView view = MakeView(def, state, 0);
    REQUIRE_FALSE(view.items.empty());
    CHECK(view.items[0].cell == here);
    CHECK(view.items[0].item == NightDef::Find(def.items, "lantern"));
    for (const FloorView &item : view.items)
        CHECK(view.cells[static_cast<std::size_t>(item.cell)].lit);
    const auto found = std::find_if(view.monsters.begin(), view.monsters.end(), [&](const MonsterView &m) { return m.cell == here; });
    REQUIRE(found != view.monsters.end());
    CHECK(found->kind == moths.kind);
}

TEST_CASE("The view marks rooms with something for the viewer: item, event, trap, bonus (T162)", "[night][view]")
{
    const NightDef def = Data();
    NightState state = Corridor(def);
    const int a = Left(def, 1), b = Left(def, 2);
    const auto mark = [&](const NightState &s, int cell) { return MakeView(def, s, 0).cells[static_cast<std::size_t>(cell)].mark; };
    CHECK(mark(state, a) == RoomMark::None);
    // 1. An armed trap and an armed event; an unresolved item room.
    NightState armed = state;
    const auto eventOf = [&](bool trap) {
        for (std::size_t e = 0; e < def.events.size(); ++e)
            if (def.events[e].trap == trap)
                return static_cast<std::int8_t>(e);
        return std::int8_t{-1};
    };
    armed.cells[static_cast<std::size_t>(a)].event = eventOf(true);
    armed.cells[static_cast<std::size_t>(b)].event = eventOf(false);
    CHECK(mark(armed, a) == RoomMark::Trap);
    CHECK(mark(armed, b) == RoomMark::Event);
    NightState item = state;
    item.cells[static_cast<std::size_t>(a)].room = static_cast<std::int8_t>(NightDef::Find(def.rooms, "armoury"));
    item.cells[static_cast<std::size_t>(a)].resolved = false;
    CHECK(mark(item, a) == RoomMark::Item);
    // 2. A bonus room: marked until the viewer has had it.
    NightState bonus = state;
    bonus.cells[static_cast<std::size_t>(a)].room = static_cast<std::int8_t>(NightDef::Find(def.rooms, "rehearsal"));
    CHECK(mark(bonus, a) == RoomMark::Bonus);
    bonus.guests[0].bonusUsed.set(static_cast<std::size_t>(a));
    CHECK(mark(bonus, a) == RoomMark::None);
}
