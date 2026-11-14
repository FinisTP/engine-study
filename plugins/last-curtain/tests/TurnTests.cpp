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

namespace
{
    // One event (or trap) row, every other room of that kind.
    NightDef OneEvent(RoomKind kind, StatKind stat, int target, Effect pass, int passAmount, Effect fail, int failAmount)
    {
        NightDef def = AllRooms(kind);
        EventDef e;
        e.id = "test";
        e.name = "Test";
        e.trap = kind == RoomKind::Trap;
        e.stat = stat;
        e.target = target;
        e.passEffect = pass;
        e.passAmount = passAmount;
        e.failEffect = fail;
        e.failAmount = failAmount;
        def.events = {e};
        return def;
    }

    int Items(const Guest &guest)
    {
        return static_cast<int>(std::count_if(guest.items.begin(), guest.items.end(), [](std::int8_t i) { return i >= 0; }));
    }
}

TEST_CASE("A check is d20 plus the stat against the target (T97)", "[night][check]")
{
    NightDef def = Data();
    def.guests[0].sanity = 5;
    NightState state = NewNight(def, 11);
    int passed = 0;
    for (int i = 0; i < 2000; ++i)
    {
        const Check check = RollCheck(def, state, 0, StatKind::Sanity, 13);
        CHECK(check.stat == 5);
        CHECK(check.target == 13);
        CHECK(check.die >= 1);
        CHECK(check.die <= 20);
        CHECK(check.passed == (check.die + 5 >= 13));
        passed += check.passed ? 1 : 0;
    }
    CHECK(passed >= 1200);
    CHECK(passed <= 1400);
}

TEST_CASE("An event applies its pass or its fail effect, once (T97)", "[night][check]")
{
    const NightDef def = OneEvent(RoomKind::Event, StatKind::Sanity, 13, Effect::Item, 1, Effect::Hp, -4);
    int passes = 0, fails = 0;
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        NightState state = NewNight(def, seed);
        const int g = Current(state);
        const Guest before = state.guests[static_cast<std::size_t>(g)];
        const Action move = First(LegalActions(def, state), ActionKind::Move);
        const std::size_t since = state.logCount;
        REQUIRE(Apply(def, state, move));
        const Guest &after = state.guests[static_cast<std::size_t>(g)];
        bool passed = false, found = false;
        for (const LogEntry &e : NewLog(state, since))
            if (e.kind == LogKind::Check)
                passed = e.a + e.other >= e.b, found = true;
        REQUIRE(found);
        if (passed)
        {
            CHECK(Items(after) == Items(before) + 1);
            ++passes;
        }
        else
        {
            CHECK(after.hp == before.hp - 4);
            ++fails;
        }
        // Resolving ended the turn; the next guest walks in and gets nothing, and may move on.
        CHECK(Current(state) != g);
        const int next = Current(state);
        const Guest nextBefore = state.guests[static_cast<std::size_t>(next)];
        REQUIRE(Apply(def, state, move));
        CHECK(Current(state) == next);
        CHECK(state.guests[static_cast<std::size_t>(next)].hp == nextBefore.hp);
        CHECK(Items(state.guests[static_cast<std::size_t>(next)]) == Items(nextBefore));
    }
    CHECK(passes > 0);
    CHECK(fails > 0);
}

TEST_CASE("A trap can bring a guest down but not kill before midnight (T97)", "[night][check]")
{
    const NightDef def = OneEvent(RoomKind::Trap, StatKind::Speed, 99, Effect::None, 0, Effect::Hp, -99);
    NightState state = NewNight(def, 2);
    const int g = Current(state);
    REQUIRE(Apply(def, state, First(LegalActions(def, state), ActionKind::Move)));
    const Guest &guest = state.guests[static_cast<std::size_t>(g)];
    CHECK(guest.down);
    CHECK_FALSE(guest.dead);
    CHECK(guest.hp == 0);
    CHECK(guest.downedBy == -1);
    // A down guest takes no turns.
    for (int i = 0; i < 20; ++i)
    {
        CHECK(Current(state) != g);
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    }
}

TEST_CASE("A relic wakes a downed guest in the same room and is used up (T97)", "[night][check]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 2);
    const int g = Current(state), other = state.order[1];
    Guest &helper = state.guests[static_cast<std::size_t>(g)];
    helper.items[0] = static_cast<std::int8_t>(Slot(def, "crucifix"));
    const Action wake{ActionKind::UseItem, 0, static_cast<std::int8_t>(other)};
    CHECK_FALSE(Apply(def, state, wake)); // nobody down
    Guest &fallen = state.guests[static_cast<std::size_t>(other)];
    fallen.down = true;
    fallen.hp = 0;
    fallen.cell = state.foyer;
    REQUIRE(Apply(def, state, wake));
    CHECK_FALSE(fallen.down);
    CHECK(fallen.hp == def.rules.guests.downedReturnHp);
    CHECK(helper.items[0] == -1);
    // Not in the same room: no.
    helper.items[0] = static_cast<std::int8_t>(Slot(def, "rosary"));
    fallen.down = true;
    fallen.hp = 0;
    Closet(def, state, state.foyer - 1);
    fallen.cell = static_cast<std::int8_t>(state.foyer - 1);
    CHECK_FALSE(Apply(def, state, wake));
}

TEST_CASE("An item room gives one item; a full guest finds it on the floor (T97)", "[night][check]")
{
    const NightDef def = AllRooms(RoomKind::Item);
    NightState state = NewNight(def, 4);
    int g = Current(state);
    REQUIRE(Apply(def, state, First(LegalActions(def, state), ActionKind::Move)));
    CHECK(Items(state.guests[static_cast<std::size_t>(g)]) == 1);
    CHECK(state.floor.empty());

    g = Current(state);
    Guest &full = state.guests[static_cast<std::size_t>(g)];
    full.items = {0, 0, 0, 0, -1, -1};
    // A different doorway, into a new item room.
    const auto legal = LegalActions(def, state);
    Action fresh{};
    for (const Action &a : legal)
        if (a.kind == ActionKind::Move && Doorway(def, state, full.cell, a.a))
            fresh = a;
    REQUIRE(fresh.kind == ActionKind::Move);
    REQUIRE(Apply(def, state, fresh));
    CHECK(Items(full) == 4);
    CHECK(state.floor.size() == 1);
}

TEST_CASE("An event can push a guest back to the room they came from (T106)", "[night][check]")
{
    for (int target : {99, 1}) // a failed roll, then a passed one: either outcome may push back
    {
        NightDef def = target == 99 ? OneEvent(RoomKind::Event, StatKind::Sanity, 99, Effect::None, 0, Effect::Back, 0)
                                    : OneEvent(RoomKind::Event, StatKind::Sanity, 1, Effect::Back, 0, Effect::None, 0);
        NightState state = NewNight(def, 5);
        const int g = Current(state);
        REQUIRE(Apply(def, state, First(LegalActions(def, state), ActionKind::Move)));
        INFO("target " << target);
        CHECK(state.guests[static_cast<std::size_t>(g)].cell == state.foyer);
        CHECK(Current(state) != g); // resolving the room ended the turn
    }
}
