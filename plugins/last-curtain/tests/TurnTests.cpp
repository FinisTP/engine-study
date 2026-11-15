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

    // A legal move from the current guest's room into a room whose event or item still waits.
    Action FreshMove(const NightDef &def, const NightState &state)
    {
        const int at = state.guests[static_cast<std::size_t>(Current(state))].cell;
        for (const Action &a : LegalActions(def, state))
            if (a.kind == ActionKind::Move && a.a < transportMove && !state.cells[static_cast<std::size_t>(Neighbour(def, at, a.a))].resolved)
                return a;
        FAIL("no move into a fresh room");
        return {};
    }
}

TEST_CASE("Order is a d20 each, ties to Speed, fixed for the night (T96)", "[night][turn]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 50; ++seed)
    {
        const NightState a = FoyerNight(def, seed), b = FoyerNight(def, seed);
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
    NightState state = FoyerNight(def, 3);
    const int first = Current(state);
    REQUIRE(first == state.order[0]);
    for (int step = 0; step < 4; ++step)
    {
        const auto legal = LegalActions(def, state);
        REQUIRE(HasKind(legal, ActionKind::Move));
        REQUIRE(Apply(def, state, First(legal, ActionKind::Move)));
    }
    // What is left is the budget less the shortest way back to where the turn began.
    const Guest &guest = state.guests[static_cast<std::size_t>(first)];
    CHECK(guest.movesLeft == 4 - Distance(def, state, guest.origin, guest.cell));
    REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    CHECK(Current(state) == state.order[1]);
}

TEST_CASE("A room with something in it stops movement (T96)", "[night][turn]")
{
    const NightDef def = AllRooms(RoomKind::Bonus);
    NightState state = FoyerNight(def, 3);
    const int g = Current(state);
    REQUIRE(Apply(def, state, FreshMove(def, state)));
    CHECK(state.guests[static_cast<std::size_t>(g)].movesLeft == 0);
    CHECK_FALSE(HasKind(LegalActions(def, state), ActionKind::Move));
}

TEST_CASE("A bonus room pays once per guest when the turn ends there (T96)", "[night][turn]")
{
    const NightDef def = AllRooms(RoomKind::Bonus);
    NightState state = FoyerNight(def, 3);
    const int g = Current(state);
    const int might = state.guests[static_cast<std::size_t>(g)].might;
    REQUIRE(Apply(def, state, FreshMove(def, state)));
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
    NightState state = FoyerNight(def, 3);
    const int trapdoor = NightDef::Find(def.rooms, "trapdoor"), catwalk = NightDef::Find(def.rooms, "catwalk");
    const int a = CellAt(def, 0, 0), b = CellAt(def, 4, 0);
    state.cells[static_cast<std::size_t>(a)] = {static_cast<std::int8_t>(trapdoor), 15, true};
    state.cells[static_cast<std::size_t>(b)] = {static_cast<std::int8_t>(catwalk), 15, true};
    const int g = Current(state);
    Guest &guest = state.guests[static_cast<std::size_t>(g)];
    guest.cell = guest.origin = static_cast<std::int8_t>(a);
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
    NightState state = FoyerNight(def, 3);
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

TEST_CASE("Twelve rounds run from 20:00, 45 minutes each, then dawn brings a new night (T96, T234)", "[night][turn]")
{
    NightDef def = Data();
    def.rules.killer.wakeRound = 99; // no killer: every guest lives to dawn
    NightState state = FoyerNight(def, 5);
    CHECK(Hour(def, state) == 20);
    CHECK(Minute(def, state) == 0);
    for (int i = 0; i < 7; ++i)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    CHECK(state.round == 2);
    CHECK(Hour(def, state) == 20);
    CHECK(Minute(def, state) == 45);
    int applied = 7;
    while (state.night == 1 && applied < 1000)
    {
        if (state.round == 12)
        {
            CHECK(Hour(def, state) == 4);
            CHECK(Minute(def, state) == 15);
        }
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
        ++applied;
    }
    CHECK(applied == 84);
    CHECK(state.round == 1);
    CHECK_FALSE(state.over);
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
    NightState state = FoyerNight(def, 11);
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
        NightState state = FoyerNight(def, seed);
        const int g = Current(state);
        const Guest before = state.guests[static_cast<std::size_t>(g)];
        const Action move = FreshMove(def, state);
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

TEST_CASE("A relic wakes a downed guest in the same room and is used up (T97)", "[night][check]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 2);
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
    NightState state = FoyerNight(def, 4);
    int g = Current(state);
    REQUIRE(Apply(def, state, FreshMove(def, state)));
    CHECK(Items(state.guests[static_cast<std::size_t>(g)]) == 1);
    CHECK(state.floor.empty());

    g = Current(state);
    Guest &full = state.guests[static_cast<std::size_t>(g)];
    full.items = {0, 0, 0, 0, -1, -1};
    // Another item room not yet searched.
    REQUIRE(Apply(def, state, FreshMove(def, state)));
    CHECK(Items(full) == 4);
    CHECK(state.floor.size() == 1);
}

TEST_CASE("An event can push a guest back to the room they came from (T106)", "[night][check]")
{
    for (int target : {99, 1}) // a failed roll, then a passed one: either outcome may push back
    {
        NightDef def = target == 99 ? OneEvent(RoomKind::Event, StatKind::Sanity, 99, Effect::None, 0, Effect::Back, 0)
                                    : OneEvent(RoomKind::Event, StatKind::Sanity, 1, Effect::Back, 0, Effect::None, 0);
        NightState state = FoyerNight(def, 5);
        const int g = Current(state);
        REQUIRE(Apply(def, state, FreshMove(def, state)));
        INFO("target " << target);
        CHECK(state.guests[static_cast<std::size_t>(g)].cell == state.foyer);
        CHECK(Current(state) != g); // resolving the room ended the turn
    }
}

namespace
{
    // A guest's first move into a room not yet resolved (the map is whole); returns that direction.
    std::int8_t OpenFirst(const NightDef &def, NightState &state)
    {
        const Action move = FreshMove(def, state);
        REQUIRE(Apply(def, state, move));
        return move.a;
    }

    bool CanMove(const NightDef &def, const NightState &state, std::int8_t d)
    {
        const auto legal = LegalActions(def, state);
        return std::find(legal.begin(), legal.end(), Action{ActionKind::Move, d}) != legal.end();
    }
}

TEST_CASE("Moves count as the shortest way back to where the turn began: out and back costs nothing (T157)", "[night][turn][moves]")
{
    const NightDef def = AllRooms(RoomKind::Plain);
    NightState state = FoyerNight(def, 3);
    Guest &guest = state.guests[static_cast<std::size_t>(Current(state))];
    const int speed = Stat(def, state, Current(state), StatKind::Speed);
    // 1. A step away costs one; the origin stays where the turn began.
    const std::int8_t d = OpenFirst(def, state), back = static_cast<std::int8_t>((d + 2) % 4);
    CHECK(guest.origin == state.foyer);
    CHECK(guest.movesLeft == speed - 1);
    // 2. Back to the foyer: free again; out and back as often as wanted (no pacing ban).
    REQUIRE(Apply(def, state, {ActionKind::Move, back}));
    CHECK(guest.movesLeft == speed);
    REQUIRE(Apply(def, state, {ActionKind::Move, d}));
    REQUIRE(Apply(def, state, {ActionKind::Move, back}));
    CHECK(guest.movesLeft == speed);
}

TEST_CASE("With no moves left a guest may still step back toward where moves count from (T157)", "[night][turn][moves]")
{
    const NightDef def = AllRooms(RoomKind::Plain);
    NightState state = FoyerNight(def, 3);
    Guest &guest = state.guests[static_cast<std::size_t>(Current(state))];
    guest.budget = 1, guest.movesLeft = 1;
    const std::int8_t d = OpenFirst(def, state), back = static_cast<std::int8_t>((d + 2) % 4);
    CHECK(guest.movesLeft == 0);
    CHECK(CanMove(def, state, back)); // back toward the start costs nothing
    const int here = guest.cell;
    for (std::int8_t other = 0; other < 4; ++other)
        if (other != back && Passage(def, state, here, other) && Distance(def, state, guest.origin, Neighbour(def, here, other)) > 1)
            CHECK_FALSE(CanMove(def, state, other)); // farther would cost one more
}

TEST_CASE("A turn has a cap on steps, so no one paces forever (T157)", "[night][turn][moves]")
{
    const NightDef def = AllRooms(RoomKind::Plain);
    NightState state = FoyerNight(def, 3);
    Guest &guest = state.guests[static_cast<std::size_t>(Current(state))];
    const std::int8_t d = OpenFirst(def, state), back = static_cast<std::int8_t>((d + 2) % 4);
    const int cap = def.rules.moves.stepsPerSpeed * Stat(def, state, Current(state), StatKind::Speed);
    for (int i = 1; i < cap; ++i) // the opening step was the first
        REQUIRE(Apply(def, state, {ActionKind::Move, i % 2 ? back : d}));
    CHECK(guest.steps == cap);
    CHECK_FALSE(CanMove(def, state, back));
    CHECK_FALSE(CanMove(def, state, d));
}

TEST_CASE("A push back leaves the guest stopped where it landed (T157)", "[night][turn][moves]")
{
    NightDef def = AllRooms(RoomKind::Event);
    for (EventDef &e : def.events)
        e.passEffect = e.failEffect = Effect::Back;
    NightState state = FoyerNight(def, 3);
    const int g = Current(state);
    OpenFirst(def, state); // an event: pushed back to the foyer, the turn over
    CHECK(state.guests[static_cast<std::size_t>(g)].cell == state.foyer);
    CHECK(state.guests[static_cast<std::size_t>(g)].movesLeft == 0);
}

namespace
{
    // Applies `move` for whoever acts and returns the log it wrote.
    std::vector<LogEntry> Enter(const NightDef &def, NightState &state, const Action &move)
    {
        const std::uint32_t since = state.logCount;
        REQUIRE(Apply(def, state, move));
        return NewLog(state, since);
    }

    bool Has(const std::vector<LogEntry> &log, LogKind kind)
    {
        return std::any_of(log.begin(), log.end(), [&](const LogEntry &e) { return e.kind == kind; });
    }
}

TEST_CASE("A persistent trap stays armed until a guest passes its roll, then is gone for everyone (T146)", "[night][check][persist]")
{
    NightDef def = OneEvent(RoomKind::Trap, StatKind::Speed, 99, Effect::None, 0, Effect::Hp, -1);
    def.events[0].persist = true;
    NightState state = FoyerNight(def, 3);
    const Action move = FreshMove(def, state);
    // 1. The first guest fails and the trap stays armed in the room.
    REQUIRE(Has(Enter(def, state, move), LogKind::Check));
    const int cell = state.guests[static_cast<std::size_t>(state.order[0])].cell;
    CHECK(state.cells[static_cast<std::size_t>(cell)].event == 0);
    CHECK_FALSE(state.cells[static_cast<std::size_t>(cell)].resolved);
    // 2. The next guest walks in and is caught again.
    const int second = Current(state);
    const int hp = state.guests[static_cast<std::size_t>(second)].hp;
    REQUIRE(Has(Enter(def, state, move), LogKind::Check));
    CHECK(state.guests[static_cast<std::size_t>(second)].hp == hp - 1);
    // 3. A guest who passes clears it for everyone.
    def.events[0].target = -99;
    const int third = Current(state);
    const std::vector<LogEntry> cleared = Enter(def, state, move);
    REQUIRE(Has(cleared, LogKind::Cleared));
    CHECK(std::find_if(cleared.begin(), cleared.end(), [](const LogEntry &e) { return e.kind == LogKind::Cleared; })->who == third);
    CHECK(state.cells[static_cast<std::size_t>(cell)].resolved);
    CHECK(state.cells[static_cast<std::size_t>(cell)].event == -1);
    // 4. The next one walks in freely and may move on.
    const int fourth = Current(state);
    CHECK_FALSE(Has(Enter(def, state, move), LogKind::Check));
    CHECK(Current(state) == fourth);
}

TEST_CASE("Meeting a persistent event's condition clears it without a roll (T146)", "[night][check][persist]")
{
    NightDef def = OneEvent(RoomKind::Event, StatKind::Sanity, 99, Effect::None, 0, Effect::Hp, -1);
    def.events[0].persist = true;
    SECTION("a stat")
    {
        def.events[0].clear = ClearKind::Stat, def.events[0].clearStat = StatKind::Speed, def.events[0].clearAmount = 1;
        NightState state = FoyerNight(def, 4);
        Guest &first = state.guests[static_cast<std::size_t>(Current(state))];
        const std::size_t nerve = static_cast<std::size_t>(NightDef::Find(def.quests, "nerve"));
        const int count = first.quest[nerve];
        const std::vector<LogEntry> log = Enter(def, state, FreshMove(def, state));
        CHECK(Has(log, LogKind::Cleared));
        CHECK_FALSE(Has(log, LogKind::Check));
        CHECK(first.quest[nerve] == count + 1); // cleared by its condition counts as passed (the endure quest)
    }
    SECTION("an item carried")
    {
        def.events[0].clear = ClearKind::Carry, def.events[0].clearItem = Slot(def, "crucifix");
        NightState state = FoyerNight(def, 4);
        const Action move = FreshMove(def, state);
        // Without it: a roll, and it stays.
        CHECK(Has(Enter(def, state, move), LogKind::Check));
        // With it: cleared at the door.
        state.guests[static_cast<std::size_t>(Current(state))].items[0] = static_cast<std::int8_t>(Slot(def, "crucifix"));
        const std::vector<LogEntry> log = Enter(def, state, move);
        CHECK(Has(log, LogKind::Cleared));
        CHECK_FALSE(Has(log, LogKind::Check));
    }
}
