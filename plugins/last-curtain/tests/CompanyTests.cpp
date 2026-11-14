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
