#include "NightTestData.hpp"

#include <algorithm>

// The battle royale's rules core (docs/design/2026-11-16-battle-royale.md, plan stage 1).
using namespace umbra::night;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }

    // Ends turns until it is `g`'s board turn (never through a battle or a chat).
    void TurnOf(const NightDef &def, NightState &state, int g)
    {
        for (int i = 0; i < 40 && Current(state) != g; ++i)
            REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
        REQUIRE(Current(state) == g);
    }

    // Kills a guest outright (the house's damage).
    void Kill(const NightDef &def, NightState &state, int g) { Hurt(def, state, g, 999, -1); }
}

TEST_CASE("The whole map is laid at the start, all reachable; no move opens a room (T228)", "[night][royale]")
{
    const NightDef def = Data();
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        const NightState state = NewNight(def, seed, 0);
        int opened = 0;
        for (int c = 0; c < maxCells; ++c)
            if (state.cells[U(c)].opened)
            {
                ++opened;
                CHECK(Steps(def, state, state.foyer, c) >= 0);
            }
        CHECK(opened >= def.rules.board.rooms - 2); // a stalled layout may stop just short
        CHECK(state.understage >= 0);
        for (const Action &a : LegalActions(def, state))
            if (a.kind == ActionKind::Move && a.a < transportMove)
                CHECK(Passage(def, state, state.guests[U(Current(state))].cell, a.a));
    }
}

TEST_CASE("Guests start far apart (T229)", "[night][royale]")
{
    const NightDef def = Data();
    int apart = 0;
    for (std::uint64_t seed = 1; seed <= 30; ++seed)
    {
        const NightState state = NewNight(def, seed, 0);
        int nearest = 99;
        for (int a = 0; a < state.count; ++a)
            for (int b = a + 1; b < state.count; ++b)
                nearest = std::min(nearest, Steps(def, state, state.guests[U(a)].cell, state.guests[U(b)].cell));
        CHECK(nearest >= 2);
        apart += nearest >= def.rules.spawn.minSteps ? 1 : 0;
    }
    CHECK(apart >= 25); // nearly always at least minSteps
}

TEST_CASE("Any guest may attack another in the same room (T230)", "[night][royale]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 3, 0);
    const int g = Current(state);
    const int other = g == 0 ? 1 : 0;
    const std::vector<Action> legal = LegalActions(def, state);
    const Action attack{ActionKind::Attack, -1, static_cast<std::int8_t>(other)};
    REQUIRE(std::find(legal.begin(), legal.end(), attack) != legal.end());
    REQUIRE(Apply(def, state, attack));
    CHECK(state.battle.active);
    bool both = false;
    for (int f = 0; f < state.battle.count; ++f)
        both = both || (state.battle.fighters[U(f)].guest == other && state.battle.fighters[U(f)].side == 1);
    CHECK(both);
}

TEST_CASE("0 HP is death: items fall, the guest becomes a ghost (T231)", "[night][royale]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 3, 0);
    state.guests[2].items[0] = static_cast<std::int8_t>(NightDef::Find(def.items, "bandage"));
    const std::size_t floor = state.floor.size();
    const int living = LivingCount(state);
    Hurt(def, state, 2, 999, 1);
    const Guest &dead = state.guests[2];
    CHECK(dead.dead);
    CHECK(dead.ghost);
    CHECK_FALSE(dead.down);
    CHECK_FALSE(Living(state, 2));
    CHECK(state.floor.size() == floor + 1);
    CHECK(LivingCount(state) == living - 1);
    bool logged = false;
    for (const LogEntry &e : NewLog(state, 0))
        logged = logged || (e.kind == LogKind::Died && e.who == 2 && e.other == 1);
    CHECK(logged);
}

TEST_CASE("One guest left alive escapes; none left, the house wins (T232)", "[night][royale]")
{
    const NightDef def = Data();
    NightState state = FoyerNight(def, 3, 0);
    for (int g = 1; g < state.count; ++g)
        Kill(def, state, g);
    CHECK(state.over);
    CHECK(state.outcome == Outcome::Escaped);
    CHECK(state.winner == 0);
    NightState none = FoyerNight(def, 3, 0);
    for (int g = 0; g < none.count; ++g)
        Kill(def, none, g);
    CHECK(none.over);
    CHECK(none.outcome == Outcome::House);
}

TEST_CASE("A ghost rises at the under-stage, attacks the living, and comes back when struck down (T233)", "[night][royale]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 5, 0);
    const int g = state.order[1];
    Kill(def, state, g);
    REQUIRE(state.guests[U(g)].ghost);
    CHECK(state.guests[U(g)].cell < 0); // out of play until its round
    // 1. It rises at the next round's start, weak, at the under-stage.
    const int round = state.round;
    for (int i = 0; i < 40 && state.round == round; ++i)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    CHECK(state.guests[U(g)].cell == state.understage);
    CHECK(state.guests[U(g)].hp == def.rules.ghost.hp);
    CHECK(Stat(def, state, g, StatKind::Might) == def.rules.ghost.might);
    CHECK(InPlay(state, g));
    // 2. On its turn: moves, attacks on the living in its room, end turn; no items.
    const int victim = state.order[0] == g ? state.order[2] : state.order[0];
    state.guests[U(victim)].cell = state.understage;
    TurnOf(def, state, g);
    for (const Action &a : LegalActions(def, state))
        CHECK((a.kind == ActionKind::Move || a.kind == ActionKind::EndTurn || (a.kind == ActionKind::Attack && a.b >= 0)));
    const std::vector<Action> legal = LegalActions(def, state);
    CHECK(std::find(legal.begin(), legal.end(), Action{ActionKind::Attack, -1, static_cast<std::int8_t>(victim)}) != legal.end());
    // 3. Struck down, it leaves the board and comes back after respawnRounds.
    Hurt(def, state, g, 999, victim);
    CHECK(state.guests[U(g)].cell < 0);
    CHECK(state.guests[U(g)].returnRound == state.round + def.rules.ghost.respawnRounds);
    CHECK(Living(state, victim));
}

TEST_CASE("At dawn with two or more alive, a new night on a new map; the living keep their gains (T234)", "[night][royale]")
{
    const NightDef def = Data();
    NightState state = NewNight(def, 9, 0);
    Kill(def, state, 3);
    state.guests[0].hp = 2, state.guests[0].might = 9;
    state.guests[0].items[0] = static_cast<std::int8_t>(NightDef::Find(def.items, "bandage"));
    const auto before = state.cells;
    for (int i = 0; i < 400 && state.night == 1 && !state.over; ++i)
        REQUIRE(Apply(def, state, LegalActions(def, state).back())); // end turns (and leave battles)
    REQUIRE(state.night == 2);
    CHECK_FALSE(state.over);
    CHECK(state.round == 1);
    CHECK(state.guests[0].hp == Stat(def, state, 0, StatKind::MaxHp));
    CHECK(state.guests[0].might == 9);
    CHECK(state.guests[0].items[0] >= 0);
    CHECK(state.guests[3].ghost);
    bool changed = false;
    for (int c = 0; c < maxCells; ++c)
        changed = changed || before[U(c)].room != state.cells[U(c)].room;
    CHECK(changed);
}
