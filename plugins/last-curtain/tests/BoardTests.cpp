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
    const NightState state = FoyerNight(def, 1);
    CHECK(CountOpened(state) == 1);
    const Cell &foyer = state.cells[static_cast<std::size_t>(Foyer(def))];
    REQUIRE(foyer.opened);
    CHECK(def.rooms[static_cast<std::size_t>(foyer.room)].kind == RoomKind::Foyer);
    CHECK(FirstDoorway(def, state).first == Foyer(def));
}

TEST_CASE("Opening every doorway builds a connected board on every seed (T95)", "[night][board]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 500; ++seed)
    {
        NightState state = FoyerNight(def, seed);
        for (;;)
        {
            const auto [cell, dir] = FirstDoorway(def, state);
            if (cell < 0)
                break;
            Open(def, state, cell, dir);
        }
        INFO("seed " << seed);
        REQUIRE(CountOpened(state) == def.rules.board.rooms);
        std::vector<int> used(def.rooms.size());
        int understage = -1;
        for (int cell = 0; cell < maxCells; ++cell)
        {
            const Cell &c = state.cells[static_cast<std::size_t>(cell)];
            if (!c.opened)
                continue;
            CHECK(Steps(def, state, Foyer(def), cell) >= 0);
            ++used[static_cast<std::size_t>(c.room)];
            if (def.rooms[static_cast<std::size_t>(c.room)].kind == RoomKind::UnderStage)
                understage = cell;
        }
        for (std::size_t r = 0; r < def.rooms.size(); ++r)
            CHECK(used[r] <= def.rooms[r].count);
        REQUIRE(understage >= 0);
        CHECK(understage == state.understage);
        CHECK(Steps(def, state, Foyer(def), understage) >= 3);
    }
}

TEST_CASE("The board is never sealed while rooms remain (T95)", "[night][board]")
{
    // Open a random doorway each time (not the first): a doorway exists until every room is placed.
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 300; ++seed)
    {
        NightState state = FoyerNight(def, seed);
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
    NightState state = FoyerNight(def, 1);
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
    NightState state = FoyerNight(def, 1);
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
    NightState state = FoyerNight(def, 1);
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
        NightState state = FoyerNight(def, seed);
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

TEST_CASE("The deck fills a 70-room night on a 9x9 board (T116)", "[night][board]")
{
    const NightDef def = Data();
    CHECK(def.rules.board.width == 9);
    CHECK(def.rules.board.height == 9);
    CHECK(def.rules.board.rooms == 70);
    CHECK(FoyerNight(def, 1).deckSize >= 85);
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

namespace
{
    // The rooms the night's drawn goals need: goal rooms and keepsake rooms.
    std::vector<int> NeededRooms(const NightDef &def, const NightState &state)
    {
        std::vector<int> rooms;
        for (int g = 0; g < state.count; ++g)
        {
            const GoalDef &goal = def.goals[static_cast<std::size_t>(state.guests[static_cast<std::size_t>(g)].goal)];
            if (goal.kind == GoalKind::Room)
                rooms.push_back(goal.target);
            if (goal.kind == GoalKind::Item)
                rooms.push_back(def.items[static_cast<std::size_t>(goal.target)].room);
        }
        return rooms;
    }
}

TEST_CASE("Seven start rooms, one guest each, connected to the foyer (T118)", "[night][board]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 500; ++seed)
    {
        INFO("seed " << seed);
        const NightState state = NewNight(def, seed);
        CHECK(state.placed == 1 + state.count);
        std::vector<int> cells;
        for (int g = 0; g < state.count; ++g)
        {
            const int cell = state.guests[static_cast<std::size_t>(g)].cell;
            CHECK(cell != state.foyer);
            CHECK(state.cells[static_cast<std::size_t>(cell)].opened);
            CHECK(Steps(def, state, state.foyer, cell) >= 0);
            CHECK(std::find(cells.begin(), cells.end(), cell) == cells.end());
            cells.push_back(cell);
        }
    }
}

TEST_CASE("No start room is a goal or keepsake room, nor the under-stage (T118)", "[night][board]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 500; ++seed)
    {
        const NightState state = NewNight(def, seed);
        const std::vector<int> needed = NeededRooms(def, state);
        for (int g = 0; g < state.count; ++g)
        {
            const int room = state.cells[static_cast<std::size_t>(state.guests[static_cast<std::size_t>(g)].cell)].room;
            CHECK(std::find(needed.begin(), needed.end(), room) == needed.end());
            CHECK(def.rooms[static_cast<std::size_t>(room)].kind != RoomKind::UnderStage);
        }
    }
}

TEST_CASE("Goal and keepsake rooms come out within the first earlyRooms rooms (T118)", "[night][board]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 500; ++seed)
    {
        INFO("seed " << seed);
        NightState state = NewNight(def, seed);
        std::uint64_t pick = seed;
        while (state.placed < def.rules.goals.earlyRooms)
        {
            std::vector<std::pair<int, int>> doorways;
            for (int cell = 0; cell < maxCells; ++cell)
                for (int dir = 0; dir < 4; ++dir)
                    if (Doorway(def, state, cell, dir))
                        doorways.push_back({cell, dir});
            REQUIRE_FALSE(doorways.empty());
            pick = pick * 6364136223846793005ULL + 1442695040888963407ULL;
            const auto [cell, dir] = doorways[(pick >> 33) % doorways.size()];
            Open(def, state, cell, dir);
        }
        for (const int room : NeededRooms(def, state))
        {
            bool placed = false;
            for (const Cell &c : state.cells)
                placed = placed || (c.opened && c.room == room);
            CHECK(placed);
        }
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
