#include "NightTestData.hpp"

#include <algorithm>

using namespace umbra::night;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }
    std::int8_t I8(int i) { return static_cast<std::int8_t>(i); }

    NightDef Quiet()
    {
        NightDef def = Data();
        def.rules.monsters.chancePercent = 0;
        return def;
    }

    bool Has(const std::vector<Action> &legal, const Action &action)
    {
        return std::find(legal.begin(), legal.end(), action) != legal.end();
    }

    // Everyone but these guests leaves the foyer for a far, dark closet (cell 0 of the grid).
    void OnlyIn(NightState &state, std::initializer_list<int> here)
    {
        Closet(Data(), state, 0);
        for (int g = 0; g < state.count; ++g)
            if (std::find(here.begin(), here.end(), g) == here.end())
                state.guests[U(g)].cell = 0;
    }

    // The last Check logged since `since`.
    LogEntry LastCheck(const NightState &state, std::size_t since)
    {
        LogEntry found{};
        for (const LogEntry &e : NewLog(state, since))
            if (e.kind == LogKind::Check)
                found = e;
        return found;
    }
}

TEST_CASE("No guest fights another before midnight (T99)", "[night][company]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 1, 0); // all seven in the foyer
    for (int i = 0; i < 70 && !state.taken.present; ++i)
    {
        for (const Action &a : LegalActions(def, state))
            CHECK_FALSE((a.kind == ActionKind::Attack && a.b >= 0));
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    }
}

TEST_CASE("Only the player persuades, at most two allies, harder against a bad bond (T99)", "[night][company]")
{
    NightDef def = Quiet();
    NightState state = NewNight(def, 1, 0);
    // Make guest 0 act first.
    while (Current(state) != 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const int a = 1, b = 2, c = 3;
    CHECK(Has(LegalActions(def, state), {ActionKind::Persuade, I8(a)}));

    // The target number reads the bond of the asked toward the asker.
    state.bonds[U(a * maxGuests + 0)] = 300;
    std::size_t since = state.logCount;
    REQUIRE(Apply(def, state, {ActionKind::Persuade, I8(a)}));
    CHECK(LastCheck(state, since).b == def.rules.persuade.target - def.rules.persuade.bond);
    const bool joined = state.guests[U(a)].group == 0;
    if (!joined)
        CHECK(state.guests[0].askedUntilRound[U(a)] == state.round + def.rules.persuade.retryRounds);

    // Next turn of guest 0.
    while (Current(state) != 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    if (!joined)
        CHECK_FALSE(Has(LegalActions(def, state), {ActionKind::Persuade, I8(a)}));
    state.bonds[U(b * maxGuests + 0)] = -300;
    since = state.logCount;
    REQUIRE(Apply(def, state, {ActionKind::Persuade, I8(b)}));
    CHECK(LastCheck(state, since).b == def.rules.persuade.target + def.rules.persuade.bond);

    // A full group asks nobody.
    state.guests[0].group = state.guests[U(a)].group = state.guests[U(b)].group = 0;
    while (Current(state) != 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    CHECK_FALSE(Has(LegalActions(def, state), {ActionKind::Persuade, I8(c)}));

    // A bot never persuades.
    while (Current(state) == 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const auto legal = LegalActions(def, state);
    CHECK(std::none_of(legal.begin(), legal.end(), [](const Action &x) { return x.kind == ActionKind::Persuade; }));
}

TEST_CASE("Allies fight beside the player and are the player's to move (T99)", "[night][company]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 1, 0);
    while (Current(state) != 0)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const int ally = 1, other = 2;
    state.guests[0].group = state.guests[U(ally)].group = 0;
    CHECK(Controls(state, 0, 0));
    CHECK(Controls(state, 0, ally));
    CHECK_FALSE(Controls(state, 0, other));
    OnlyIn(state, {0, ally});
    state.monsters.push_back({0, state.foyer, 99, 99, 1, 1});
    REQUIRE(Apply(def, state, {ActionKind::Attack, 0}));
    REQUIRE(state.battle.active);
    bool allyBeside = false;
    for (int f = 0; f < state.battle.count; ++f)
        allyBeside = allyBeside || (state.battle.fighters[U(f)].guest == ally && state.battle.fighters[U(f)].side == 0);
    CHECK(allyBeside);
}


namespace
{
    // Every room but the foyer and the under-stage is an omen room with four doors.
    NightDef OmenRooms()
    {
        NightDef def = Quiet();
        for (RoomDef &room : def.rooms)
            if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
                room.kind = RoomKind::Omen, room.doors = 15;
        return def;
    }

    // The current guest walks through a new doorway (into an omen room).
    void WalkIn(const NightDef &def, NightState &state)
    {
        const int g = Current(state);
        for (std::int8_t d = 0; d < 4; ++d)
            if (Doorway(def, state, state.guests[U(g)].cell, d))
            {
                REQUIRE(Apply(def, state, {ActionKind::Move, d}));
                return;
            }
        FAIL("no doorway");
    }
}

TEST_CASE("An omen gives its gift and costs a roll against a rising target (T100)", "[night][omen]")
{
    NightDef def = OmenRooms();
    def.rules.omens.targets = {1, 1, 1}; // a d20 never rolls below 1: no omen turns anyone here
    NightState state = NewNight(def, 3);
    for (std::size_t n = 0; n < 3; ++n)
    {
        const int g = Current(state);
        WalkIn(def, state);
        const auto legal = LegalActions(def, state);
        REQUIRE(legal.size() == 2);
        const Cell &cell = state.cells[U(state.guests[U(g)].cell)];
        REQUIRE(cell.omen >= 0);
        const OmenDef &omen = def.omens[U(cell.omen)];
        const int sanity = Stat(def, state, g, StatKind::Sanity), maxHp = Stat(def, state, g, StatKind::MaxHp);
        const int hp = state.guests[U(g)].hp, might = Stat(def, state, g, StatKind::Might);
        const std::size_t since = state.logCount;
        const int round = state.round;
        REQUIRE(Apply(def, state, {ActionKind::TakeOmen}));
        if (state.taken.present)
            break; // an unlucky roll: the guest turned
        REQUIRE(state.round == round);
        const LogEntry check = LastCheck(state, since);
        CHECK(check.b == def.rules.omens.targets[n] + def.rules.omens.perHour * ((state.round - 1) * 30 / 60));
        CHECK(check.other == 0); // a plain d20: the same odds for everyone
        CHECK(Stat(def, state, g, StatKind::Sanity) == sanity + omen.sanity);
        CHECK(Stat(def, state, g, StatKind::Might) == might + omen.might);
        CHECK(Stat(def, state, g, StatKind::MaxHp) == maxHp + omen.maxhp);
        CHECK(state.guests[U(g)].hp == hp + omen.maxhp);
        CHECK(state.omensTaken == static_cast<int>(n) + 1);
        CHECK(Current(state) != g); // taking ends the turn
    }
}

TEST_CASE("The curse target rises with omens taken and with the hour (T100)", "[night][omen]")
{
    NightDef def = OmenRooms();
    def.rules.omens.targets = {2, 5};
    def.rules.omens.perHour = 3;
    NightState state = NewNight(def, 3);
    CHECK(CurseTarget(def, state) == 2);
    state.omensTaken = 1;
    CHECK(CurseTarget(def, state) == 5);
    state.omensTaken = 4; // the last target repeats
    CHECK(CurseTarget(def, state) == 5);
    state.round = 5; // 21:00, two hours in
    CHECK(CurseTarget(def, state) == 5 + 2 * 3);
}

TEST_CASE("A failed curse roll turns that guest at once and the hunt starts at midnight (T100)", "[night][omen]")
{
    NightDef def = OmenRooms();
    def.rules.omens.targets = {99};
    NightState state = NewNight(def, 3);
    const int taker = Current(state);
    WalkIn(def, state);
    REQUIRE(Apply(def, state, {ActionKind::TakeOmen}));
    REQUIRE(state.taken.present);
    CHECK(state.taken.guest == taker);
    CHECK(state.round == def.rules.clock.turnAfterRound + 1);
    CHECK(Hour(def, state) == 0);
    CHECK(state.guests[U(taker)].cell == state.understage);
    for (int g = 0; g < state.count; ++g)
        if (g != taker)
            CHECK(state.guests[U(g)].cell == state.foyer);
    CHECK(Current(state) == state.order[0]); // the company moves first
    CHECK(state.order[U(state.count - 1)] == taker);
}

TEST_CASE("Leaving an omen leaves it for the next guest (T100)", "[night][omen]")
{
    const NightDef def = OmenRooms();
    NightState state = NewNight(def, 3);
    const int first = Current(state);
    WalkIn(def, state);
    const int cell = state.guests[U(first)].cell;
    const int omen = state.cells[U(cell)].omen;
    REQUIRE(Apply(def, state, {ActionKind::LeaveOmen}));
    CHECK(state.guests[U(first)].omens == 0);
    CHECK(Current(state) != first);
    // The next guest walks into the same room through the passage now open.
    const int next = Current(state);
    std::int8_t way = -1;
    for (std::int8_t d = 0; d < 4; ++d)
        if (Neighbour(def, state.guests[U(next)].cell, d) == cell && Passage(def, state, state.guests[U(next)].cell, d))
            way = d;
    REQUIRE(way >= 0);
    REQUIRE(Apply(def, state, {ActionKind::Move, way}));
    REQUIRE(Apply(def, state, {ActionKind::TakeOmen}));
    CHECK(((state.guests[U(next)].omens >> omen) & 1u) == 1u);
}

