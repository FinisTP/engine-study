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

    // Strike the first enemy until the battle is over (joiners stay out).
    void FightOut(const NightDef &def, NightState &state)
    {
        for (int i = 0; i < 200 && state.battle.active; ++i)
        {
            const auto legal = LegalActions(def, state);
            auto pick = std::find_if(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::Strike; });
            REQUIRE(Apply(def, state, pick != legal.end() ? *pick : Action{ActionKind::EndTurn}));
        }
    }

    // Everyone but these guests leaves the foyer for a far, dark corner (cell 0 of the grid, not opened).
    void OnlyIn(NightState &state, std::initializer_list<int> here)
    {
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

TEST_CASE("Attacking a guest is slow, pays no XP and is seen (T99)", "[night][company]")
{
    NightDef def = Quiet();
    for (GuestDef &g : def.guests)
        g.hp = 30, g.might = 4, g.sanity = 4, g.weapon = Weapon::Physical;
    NightState state = NewNight(def, 1);
    const int attacker = Current(state), defender = state.order[1], watcher = state.order[2], dark = state.order[3];
    OnlyIn(state, {attacker, defender, watcher});
    state.guests[U(dark)].cell = 0;
    const int seen = Bond(state, watcher, attacker), unseen = Bond(state, dark, attacker);
    REQUIRE(Apply(def, state, {ActionKind::Attack, -1, I8(defender)}));
    // The watcher is a bot in the room: it may join; it stays out.
    if (state.battle.active && Current(state) == watcher)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    FightOut(def, state);
    CHECK_FALSE(state.guests[U(attacker)].down);
    CHECK_FALSE(state.guests[U(defender)].down);
    CHECK(state.guests[U(attacker)].xp == 0);
    CHECK(state.guests[U(defender)].xp == 0);
    CHECK(Bond(state, watcher, attacker) == std::max(-1000, seen - def.rules.guests.attackBondLoss));
    CHECK(Bond(state, dark, attacker) == unseen);
}

TEST_CASE("A guest brought down by a guest stays down until midnight (T99)", "[night][company]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 1);
    const int attacker = Current(state), defender = state.order[1];
    OnlyIn(state, {attacker, defender});
    state.guests[U(defender)].hp = 1;
    REQUIRE(Apply(def, state, {ActionKind::Attack, -1, I8(defender)}));
    FightOut(def, state);
    CHECK(state.guests[U(defender)].down);
    CHECK(state.guests[U(defender)].downedBy == attacker);
    for (int i = 0; i < 14; ++i)
    {
        CHECK(Current(state) != defender);
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
    OnlyIn(state, {0, ally, other});
    state.guests[U(other)].hp = 99;
    REQUIRE(Apply(def, state, {ActionKind::Attack, -1, I8(other)}));
    REQUIRE(state.battle.active);
    bool allyBeside = false;
    for (int f = 0; f < state.battle.count; ++f)
        allyBeside = allyBeside || (state.battle.fighters[U(f)].guest == ally && state.battle.fighters[U(f)].side == 0);
    CHECK(allyBeside);
}

TEST_CASE("A bot in the room may be offered the defender's side (T99)", "[night][company]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 1);
    const int attacker = Current(state), defender = state.order[1], bystander = state.order[2];
    OnlyIn(state, {attacker, defender, bystander});
    CHECK(Joiners(def, state, attacker, defender) == std::vector<int>{bystander});
    REQUIRE(Apply(def, state, {ActionKind::Attack, -1, I8(defender)}));
    REQUIRE(Current(state) == bystander);
    const auto legal = LegalActions(def, state);
    REQUIRE(legal.size() == 2);
    CHECK(Has(legal, {ActionKind::Join}));
    CHECK(Has(legal, {ActionKind::EndTurn}));
    REQUIRE(Apply(def, state, {ActionKind::Join}));
    bool joined = false;
    for (int f = 0; f < state.battle.count; ++f)
        joined = joined || (state.battle.fighters[U(f)].guest == bystander && state.battle.fighters[U(f)].side == 1);
    CHECK(joined);
    CHECK(Current(state) == attacker); // the battle begins with the starter
    FightOut(def, state);
    // Joining did not use the bystander's own board turn.
    while (Current(state) >= 0 && Current(state) != bystander && state.round == 1)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    CHECK(Current(state) == bystander);
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
    const NightDef def = OmenRooms();
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
        REQUIRE(Apply(def, state, {ActionKind::TakeOmen}));
        const LogEntry check = LastCheck(state, since);
        CHECK(check.b == def.rules.omens.targets[n]);
        CHECK(check.other == sanity); // the Sanity before the gift
        CHECK(Stat(def, state, g, StatKind::Sanity) == sanity + omen.sanity);
        CHECK(Stat(def, state, g, StatKind::Might) == might + omen.might);
        CHECK(Stat(def, state, g, StatKind::MaxHp) == maxHp + omen.maxhp);
        CHECK(state.guests[U(g)].hp == hp + omen.maxhp);
        CHECK(state.omensTaken == static_cast<int>(n) + 1);
        CHECK(Current(state) != g); // taking ends the turn
    }
}

TEST_CASE("The first failure marks, in secret (T100)", "[night][omen]")
{
    NightDef def = OmenRooms();
    def.rules.omens.targets = {99};
    NightState state = NewNight(def, 3);
    const int taker = Current(state), watcher = state.order[1], dark = state.order[2];
    state.guests[U(dark)].cell = 0; // an unopened corner: no light reaches it
    WalkIn(def, state);
    REQUIRE(Apply(def, state, {ActionKind::TakeOmen}));
    CHECK(state.marked == taker);
    CHECK(Knows(state, watcher, taker).omen);
    CHECK(Knows(state, watcher, taker).fail);
    CHECK_FALSE(Knows(state, dark, taker).omen);
    CHECK_FALSE(Knows(state, dark, taker).fail);
    // A later failure does not move the mark.
    WalkIn(def, state);
    REQUIRE(Apply(def, state, {ActionKind::TakeOmen}));
    CHECK(state.marked == taker);
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

TEST_CASE("Bringing the marked guest down by a guest's hand wins the night at once (T100)", "[night][omen]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 1);
    const int attacker = Current(state), marked = state.order[1];
    state.marked = static_cast<std::int8_t>(marked);
    OnlyIn(state, {attacker, marked});
    NightState trap = state;
    state.guests[U(marked)].hp = 1;
    REQUIRE(Apply(def, state, {ActionKind::Attack, -1, I8(marked)}));
    FightOut(def, state);
    CHECK(Result(state) == Outcome::CompanyEarly);
    CHECK(Current(state) == -1);
    // The same fall to the house leaves the night running.
    Hurt(def, trap, marked, 999, -1);
    CHECK(trap.guests[U(marked)].down);
    CHECK(Result(trap) == Outcome::Running);
}
