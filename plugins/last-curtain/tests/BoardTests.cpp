#include "NightTestData.hpp"

#include <catch2/catch_test_macros.hpp>

#include <tuple>

using namespace umbra::night;

namespace
{
    int Foyer(const NightDef &def) { return CellAt(def, def.rules.board.width / 2, def.rules.board.height - 1); }

    int CountOpened(const NightState &state)
    {
        int n = 0;
        for (const Cell &c : state.cells)
            n += c.opened ? 1 : 0;
        return n;
    }

    // The first doorway on the board, as (cell, direction), or (-1, -1).
    std::pair<int, int> FirstDoorway(const NightDef &def, const NightState &state)
    {
        for (int cell = 0; cell < maxCells; ++cell)
            for (int dir = 0; dir < 4; ++dir)
                if (state.cells[static_cast<std::size_t>(cell)].opened && Doorway(def, state, cell, dir))
                    return {cell, dir};
        return {-1, -1};
    }

    // A board by hand: the cells listed are opened with these doors, the room is the first plain row.
    void Hand(const NightDef &def, NightState &state, std::initializer_list<std::pair<int, std::uint8_t>> cells)
    {
        state.cells = {};
        const int plain = NightDef::Find(def.rooms, "auditorium");
        for (auto [cell, doors] : cells)
            state.cells[static_cast<std::size_t>(cell)] = {static_cast<std::int8_t>(plain), doors, true};
    }

    constexpr std::uint8_t N = 1, E = 2, S = 4, W = 8;
}

TEST_CASE("A new night has only the foyer, bottom centre, with a doorway (T95)", "[night][board]")
{
    const NightDef def = Data();
    const NightState state = BareFoyer(def, 1);
    CHECK(CountOpened(state) == 1);
    const Cell &foyer = state.cells[static_cast<std::size_t>(Foyer(def))];
    REQUIRE(foyer.opened);
    CHECK(def.rooms[static_cast<std::size_t>(foyer.room)].kind == RoomKind::Foyer);
    CHECK(FirstDoorway(def, state).first == Foyer(def));
}

TEST_CASE("The board is never sealed while rooms remain (T95)", "[night][board]")
{
    // Open a random doorway each time (not the first): a doorway exists until every room is placed.
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 300; ++seed)
    {
        NightState state = BareFoyer(def, seed);
        std::uint64_t pick = seed;
        while (CountOpened(state) < def.rules.board.rooms)
        {
            std::vector<std::pair<int, int>> doorways;
            for (int cell = 0; cell < maxCells; ++cell)
                for (int dir = 0; dir < 4; ++dir)
                    if (Doorway(def, state, cell, dir))
                        doorways.push_back({cell, dir});
            INFO("seed " << seed);
            REQUIRE_FALSE(doorways.empty());
            pick = pick * 6364136223846793005ULL + 1442695040888963407ULL;
            const auto [cell, dir] = doorways[(pick >> 33) % doorways.size()];
            Open(def, state, cell, dir);
        }
        CHECK(FirstDoorway(def, state).first == -1);
    }
}

TEST_CASE("Doors must face each other to pass (T95)", "[night][board]")
{
    const NightDef def = Data();
    NightState state = BareFoyer(def, 1);
    const int a = CellAt(def, 1, 1), b = CellAt(def, 2, 1);
    Hand(def, state, {{a, E}, {b, N}});
    CHECK_FALSE(Passage(def, state, a, 1));
    CHECK(Steps(def, state, a, b) == -1);
    Hand(def, state, {{a, E}, {b, W}});
    CHECK(Passage(def, state, a, 1));
    CHECK(Passage(def, state, b, 3));
    CHECK(Steps(def, state, a, b) == 1);
}

TEST_CASE("Light reaches by steps through doors, not through walls (T95)", "[night][board]")
{
    const NightDef def = Data();
    NightState state = BareFoyer(def, 1);
    // A corridor along row 0: cells 0..4 then down to (4,1).
    const int c0 = CellAt(def, 0, 0), c1 = CellAt(def, 1, 0), c2 = CellAt(def, 2, 0), c3 = CellAt(def, 3, 0),
              c4 = CellAt(def, 4, 0), c5 = CellAt(def, 4, 1);
    Hand(def, state, {{c0, E}, {c1, E | W}, {c2, E | W}, {c3, E | W}, {c4, W | S}, {c5, N}});
    CHECK(Lit(def, state, c0, c4, 4));
    CHECK_FALSE(Lit(def, state, c0, c5, 4));
    // A U-shaped path: (0,1) is next to (0,0) on the grid but 7 steps round a wall.
    const int u0 = CellAt(def, 0, 0), u1 = CellAt(def, 0, 1);
    Hand(def, state, {{u0, E}, {c1, W | S}, {CellAt(def, 1, 1), N | S}, {CellAt(def, 1, 2), N | W},
                      {CellAt(def, 0, 2), E | N}, {u1, S}});
    CHECK(Steps(def, state, u0, u1) == 5);
    CHECK_FALSE(Lit(def, state, u0, u1, 4));
}

TEST_CASE("A line is straight and unbroken (T95)", "[night][board]")
{
    const NightDef def = Data();
    NightState state = BareFoyer(def, 1);
    const int a = CellAt(def, 0, 2), b = CellAt(def, 1, 2), c = CellAt(def, 2, 2), d = CellAt(def, 2, 3);
    Hand(def, state, {{a, E}, {b, E | W}, {c, W | S}, {d, N}});
    CHECK(InLine(def, state, a, c));
    CHECK(InLine(def, state, c, a));
    CHECK_FALSE(InLine(def, state, a, d)); // a corner
    Hand(def, state, {{a, E}, {b, W}, {c, W}});
    CHECK_FALSE(InLine(def, state, a, c)); // a missing door between b and c
}

TEST_CASE("The same seed builds the same board (T95)", "[night][board]")
{
    const NightDef def = Data();
    auto build = [&](std::uint64_t seed) {
        NightState state = BareFoyer(def, seed);
        for (auto [cell, dir] = FirstDoorway(def, state); cell >= 0; std::tie(cell, dir) = FirstDoorway(def, state))
            Open(def, state, cell, dir);
        return state;
    };
    const NightState a = build(7), b = build(7), c = build(8);
    bool same = true, differs = false;
    for (int i = 0; i < maxCells; ++i)
    {
        same = same && a.cells[static_cast<std::size_t>(i)].room == b.cells[static_cast<std::size_t>(i)].room &&
               a.cells[static_cast<std::size_t>(i)].doors == b.cells[static_cast<std::size_t>(i)].doors;
        differs = differs || a.cells[static_cast<std::size_t>(i)].room != c.cells[static_cast<std::size_t>(i)].room;
    }
    CHECK(same);
    CHECK(differs);
}

TEST_CASE("Every event and trap room has an event to draw (T116)", "[night][board]")
{
    const NightDef def = Data();
    int eventRooms = 0, trapRooms = 0, events = 0, traps = 0;
    for (const RoomDef &room : def.rooms)
        eventRooms += room.kind == RoomKind::Event ? room.count : 0, trapRooms += room.kind == RoomKind::Trap ? room.count : 0;
    for (const EventDef &e : def.events)
        (e.trap ? traps : events) += 1;
    CHECK(eventRooms <= events);
    CHECK(trapRooms <= traps);
}

TEST_CASE("No start room is the under-stage, the ghosts' tile (T118)", "[night][board]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 200; ++seed)
    {
        const NightState state = NewNight(def, seed);
        for (int g = 0; g < state.count; ++g)
            CHECK(state.guests[static_cast<std::size_t>(g)].cell != state.understage);
    }
}

TEST_CASE("A start room gives nothing on first entry (T118)", "[night][board]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 100; ++seed)
    {
        const NightState state = NewNight(def, seed);
        for (int g = 0; g < state.count; ++g)
        {
            const Guest &guest = state.guests[static_cast<std::size_t>(g)];
            CHECK(state.cells[static_cast<std::size_t>(guest.cell)].resolved);
            CHECK(guest.bonusUsed[static_cast<std::size_t>(guest.cell)]);
            CHECK(std::all_of(guest.items.begin(), guest.items.end(), [](std::int8_t i) { return i < 0; }));
        }
    }
}

