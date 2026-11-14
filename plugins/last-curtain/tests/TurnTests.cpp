#include "NightTestData.hpp"

#include "../modules/night/src/NightInner.hpp"

#include <algorithm>

using namespace umbra::night;

namespace
{
    bool HasKind(const std::vector<Action> &actions, ActionKind kind)
    {
        return std::any_of(actions.begin(), actions.end(), [&](const Action &a) { return a.kind == kind; });
    }

    Action First(const std::vector<Action> &actions, ActionKind kind)
    {
        for (const Action &a : actions)
            if (a.kind == kind)
                return a;
        FAIL("no action of that kind");
        return {};
    }

    // Every room but the foyer and the under-stage becomes this kind, with four doors.
    NightDef AllRooms(RoomKind kind)
    {
        NightDef def = Data();
        for (RoomDef &room : def.rooms)
            if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            {
                room.kind = kind;
                room.doors = 15;
                room.stat = StatKind::Might;
                room.amount = 1;
            }
        return def;
    }

    int Slot(const NightDef &def, const char *item) { return NightDef::Find(def.items, item); }
}

TEST_CASE("Order is a d20 each, ties to Speed, fixed for the night (T96)", "[night][turn]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 50; ++seed)
    {
        const NightState a = NewNight(def, seed), b = NewNight(def, seed);
        std::array<int, maxGuests> seen{};
        for (std::int8_t g : a.order)
            ++seen[static_cast<std::size_t>(g)];
        CHECK(std::all_of(seen.begin(), seen.end(), [](int n) { return n == 1; }));
        CHECK(a.order == b.order);
    }
    // The tie rule: high roll first, then higher Speed, then the lower guest index.
    const auto order = inner::Order({12, 20, 12, 5, 12, 20, 1}, {3, 2, 6, 9, 6, 4, 1}, 7);
    const std::array<std::int8_t, maxGuests> expected{5, 1, 2, 4, 0, 3, 6};
    CHECK(order == expected);
}

TEST_CASE("A guest moves up to Speed and plain rooms do not stop it (T96)", "[night][turn]")
{
    NightDef def = AllRooms(RoomKind::Plain);
    for (GuestDef &g : def.guests)
        g.speed = 4;
    NightState state = NewNight(def, 3);
    const int first = Current(state);
    REQUIRE(first == state.order[0]);
    for (int step = 0; step < 4; ++step)
    {
        const auto legal = LegalActions(def, state);
        REQUIRE(HasKind(legal, ActionKind::Move));
        REQUIRE(Apply(def, state, First(legal, ActionKind::Move)));
    }
    CHECK_FALSE(HasKind(LegalActions(def, state), ActionKind::Move));
    CHECK_FALSE(Apply(def, state, {ActionKind::Move, 0}));
    REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    CHECK(Current(state) == state.order[1]);
}

TEST_CASE("A room with something in it stops movement (T96)", "[night][turn]")
{
    const NightDef def = AllRooms(RoomKind::Bonus);
    NightState state = NewNight(def, 3);
    const int g = Current(state);
    REQUIRE(Apply(def, state, First(LegalActions(def, state), ActionKind::Move)));
    CHECK(state.guests[static_cast<std::size_t>(g)].movesLeft == 0);
    CHECK_FALSE(HasKind(LegalActions(def, state), ActionKind::Move));
}

TEST_CASE("A bonus room pays once per guest when the turn ends there (T96)", "[night][turn]")
{
    const NightDef def = AllRooms(RoomKind::Bonus);
    NightState state = NewNight(def, 3);
    const int g = Current(state);
    const int might = state.guests[static_cast<std::size_t>(g)].might;
    REQUIRE(Apply(def, state, First(LegalActions(def, state), ActionKind::Move)));
    REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    CHECK(state.guests[static_cast<std::size_t>(g)].might == might + 1);
    // Everyone else passes; the same guest ends a second turn there: nothing more.
    while (Current(state) != g)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    CHECK(state.guests[static_cast<std::size_t>(g)].might == might + 1);
}

TEST_CASE("Transport rooms are a pair (T96)", "[night][turn]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 3);
    const int trapdoor = NightDef::Find(def.rooms, "trapdoor"), catwalk = NightDef::Find(def.rooms, "catwalk");
    const int a = CellAt(def, 0, 0), b = CellAt(def, 4, 0);
    state.cells[static_cast<std::size_t>(a)] = {static_cast<std::int8_t>(trapdoor), 15, true};
    state.cells[static_cast<std::size_t>(b)] = {static_cast<std::int8_t>(catwalk), 15, true};
    const int g = Current(state);
    Guest &guest = state.guests[static_cast<std::size_t>(g)];
    guest.cell = static_cast<std::int8_t>(a);
    const int moves = guest.movesLeft;
    REQUIRE(Apply(def, state, {ActionKind::Move, transportMove}));
    CHECK(guest.cell == b);
    CHECK(guest.movesLeft == moves - 1);
    // Only one of the pair placed: no transport move.
    state.cells[static_cast<std::size_t>(a)] = {};
    CHECK_FALSE(Apply(def, state, {ActionKind::Move, transportMove}));
}

TEST_CASE("Items: four carried, any number on a floor, all free (T96)", "[night][turn]")
{
    const NightDef def = AllRooms(RoomKind::Plain);
    NightState state = NewNight(def, 3);
    const int g = Current(state);
    Guest &guest = state.guests[static_cast<std::size_t>(g)];
    guest.items = {static_cast<std::int8_t>(Slot(def, "shoes")), static_cast<std::int8_t>(Slot(def, "bandage")),
                   static_cast<std::int8_t>(Slot(def, "fan")), static_cast<std::int8_t>(Slot(def, "sword")), -1, -1};
    state.floor.push_back({guest.cell, static_cast<std::int8_t>(Slot(def, "tonic"))});
    const int speed = Stat(def, state, g, StatKind::Speed);
    CHECK(speed == def.guests[static_cast<std::size_t>(g)].speed + 1);
    CHECK_FALSE(HasKind(LegalActions(def, state), ActionKind::PickUp));

    REQUIRE(Apply(def, state, {ActionKind::DropItem, 0})); // the shoes
    CHECK(Stat(def, state, g, StatKind::Speed) == speed - 1);
    CHECK(state.floor.size() == 2);
    CHECK(HasKind(LegalActions(def, state), ActionKind::PickUp));
    CHECK(HasKind(LegalActions(def, state), ActionKind::Move));

    // A bandage heals 6, never above max HP; it is used up.
    guest.hp = static_cast<std::int16_t>(Stat(def, state, g, StatKind::MaxHp) - 2);
    REQUIRE(Apply(def, state, {ActionKind::UseItem, 1}));
    CHECK(guest.hp == Stat(def, state, g, StatKind::MaxHp));
    CHECK(guest.items[1] == -1);
    // A worn item is not "used".
    CHECK_FALSE(Apply(def, state, {ActionKind::UseItem, 2}));
    CHECK(Current(state) == g);
}

TEST_CASE("Twenty rounds run from 19:00 to 04:30 (T96)", "[night][turn]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 5);
    CHECK(Hour(def, state) == 19);
    CHECK(Minute(def, state) == 0);
    for (int i = 0; i < 7; ++i)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    CHECK(state.round == 2);
    CHECK(Hour(def, state) == 19);
    CHECK(Minute(def, state) == 30);
    int applied = 7;
    while (Current(state) >= 0 && applied < 1000)
    {
        if (state.round == 20)
        {
            CHECK(Hour(def, state) == 4);
            CHECK(Minute(def, state) == 30);
        }
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
        ++applied;
    }
    CHECK(applied == 140);
    CHECK(LegalActions(def, state).empty());
}
