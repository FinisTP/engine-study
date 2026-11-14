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

    // Every guest passes until the hunt begins (round turnAfterRound + 1).
    void PassToMidnight(const NightDef &def, NightState &state)
    {
        while (Current(state) >= 0 && state.round <= def.rules.clock.turnAfterRound)
        {
            const auto legal = LegalActions(def, state);
            REQUIRE(Apply(def, state, legal[0].kind == ActionKind::LevelUp ? legal[0] : Action{ActionKind::EndTurn}));
        }
    }

    // A board built far enough to hold the under-stage.
    NightState Built(const NightDef &def, std::uint64_t seed, int player = -1)
    {
        NightState state = NewNight(def, seed, player);
        for (;;)
        {
            int cell = -1, dir = -1;
            for (int c = 0; c < maxCells && cell < 0; ++c)
                for (int d = 0; d < 4; ++d)
                    if (Doorway(def, state, c, d))
                    {
                        cell = c, dir = d;
                        break;
                    }
            if (cell < 0)
                break;
            Open(def, state, cell, dir);
        }
        return state;
    }

    void FightOut(const NightDef &def, NightState &state)
    {
        for (int i = 0; i < 200 && state.battle.active; ++i)
        {
            const auto legal = LegalActions(def, state);
            auto pick = std::find_if(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::Strike; });
            REQUIRE(Apply(def, state, pick != legal.end() ? *pick : legal.back()));
        }
    }

    int TakenGuest(const NightState &state) { return state.taken.present ? state.taken.guest : -1; }
}

TEST_CASE("At midnight the marked guest becomes the Taken and the rest gather (T101)", "[night][hunt]")
{
    const NightDef def = Quiet();
    NightState state = Built(def, 4);
    const int marked = 2, trapped = 3, struck = 4;
    state.marked = I8(marked);
    state.guests[U(trapped)].down = true, state.guests[U(trapped)].hp = 0;
    state.guests[U(struck)].down = true, state.guests[U(struck)].hp = 0, state.guests[U(struck)].downedBy = 1;
    state.guests[U(5)].cell = state.understage;
    PassToMidnight(def, state);
    REQUIRE(state.taken.present);
    CHECK(TakenGuest(state) == marked);
    CHECK(state.guests[U(marked)].cell == state.understage);
    CHECK(state.guests[U(marked)].hp == def.guests[U(marked)].takenHp);
    CHECK(IsTaken(state, marked));
    for (int g = 0; g < state.count; ++g)
        if (g != marked)
        {
            CHECK(state.guests[U(g)].cell == state.foyer);
            CHECK_FALSE(state.guests[U(g)].down);
        }
    CHECK(state.guests[U(trapped)].hp == def.rules.guests.downedReturnHp);
    CHECK(state.guests[U(struck)].hp == def.rules.guests.downedReturnHp);
    CHECK(state.order[U(state.count - 1)] == marked); // the Taken acts last
    CHECK(Stat(def, state, marked, StatKind::Might) == def.guests[U(marked)].takenMight);
}

TEST_CASE("With no failed roll, chance marks an omen holder; with no omen, anyone (T101)", "[night][hunt]")
{
    const NightDef def = Quiet();
    int heavy = 0;
    for (std::uint64_t seed = 1; seed <= 300; ++seed)
    {
        NightState state = Built(def, seed);
        state.guests[1].omens = 0b1;
        state.guests[5].omens = 0b10110;
        PassToMidnight(def, state);
        REQUIRE((TakenGuest(state) == 1 || TakenGuest(state) == 5));
        heavy += TakenGuest(state) == 5 ? 1 : 0;
    }
    CHECK(heavy >= 195);
    CHECK(heavy <= 255);
    std::array<int, maxGuests> chosen{};
    for (std::uint64_t seed = 1; seed <= 300; ++seed)
    {
        NightState state = Built(def, seed, 0);
        PassToMidnight(def, state);
        ++chosen[U(TakenGuest(state))];
    }
    CHECK(std::all_of(chosen.begin(), chosen.end(), [](int n) { return n > 0; }));
}

TEST_CASE("A marked guest felled by the house returns at full strength (T101)", "[night][hunt]")
{
    const NightDef def = Quiet();
    NightState state = Built(def, 4);
    state.marked = 3;
    Hurt(def, state, 3, 999, -1);
    REQUIRE(state.guests[3].down);
    CHECK(Result(state) == Outcome::Running);
    PassToMidnight(def, state);
    CHECK(TakenGuest(state) == 3);
    CHECK_FALSE(state.guests[3].down);
    CHECK(state.guests[3].hp == def.guests[3].takenHp);
}

TEST_CASE("A marked ally leaves the player's group; a marked player controls the Taken (T101)", "[night][hunt]")
{
    const NightDef def = Quiet();
    NightState state = Built(def, 4, 0);
    state.guests[0].group = state.guests[2].group = 0;
    state.marked = 2;
    PassToMidnight(def, state);
    CHECK(state.guests[2].group == -1);
    CHECK_FALSE(Controls(state, 0, 2));

    NightState mine = Built(def, 4, 0);
    mine.marked = 0;
    PassToMidnight(def, mine);
    CHECK(TakenGuest(mine) == 0);
    CHECK(Controls(mine, 0, 0));
}

TEST_CASE("After midnight zero HP is death and items fall (T101)", "[night][hunt]")
{
    const NightDef def = Quiet();
    NightState state = Built(def, 4);
    state.marked = 6;
    PassToMidnight(def, state);
    Guest &victim = state.guests[1];
    victim.items[0] = 0;
    const std::size_t floor = state.floor.size();
    Hurt(def, state, 1, 999, 6);
    CHECK(victim.dead);
    CHECK(victim.items[0] == -1);
    CHECK(state.floor.size() == floor + 1);
}

TEST_CASE("Battle on contact: three rounds, the Taken's HP carries over (T101)", "[night][hunt]")
{
    NightDef def = Quiet();
    for (GuestDef &g : def.guests)
        g.hp = 60;
    NightState state = Built(def, 4);
    state.marked = 6;
    PassToMidnight(def, state);
    const int taken = TakenGuest(state);
    // Put the Taken next to the foyer by hand and walk the first guest in.
    int start = state.guests[U(taken)].hp;
    for (int fight = 0; fight < 2; ++fight)
    {
        while (Current(state) == taken || state.battle.active)
            REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
        state.guests[U(taken)].cell = state.foyer;
        const int g = Current(state);
        REQUIRE(Apply(def, state, {ActionKind::Attack, -1, I8(taken)}));
        REQUIRE(state.battle.active);
        int company = 0;
        for (int f = 0; f < state.battle.count; ++f)
            company += state.battle.fighters[U(f)].guest >= 0 && state.battle.fighters[U(f)].guest != taken &&
                       state.battle.fighters[U(f)].side == 0;
        CHECK(company == 6); // every company guest in the foyer fights
        (void)g;
        FightOut(def, state);
        CHECK(state.guests[U(taken)].hp < start);
        start = state.guests[U(taken)].hp;
        if (Result(state) != Outcome::Running)
            break;
    }
}

TEST_CASE("Monsters and traps side with the Taken (T101)", "[night][hunt]")
{
    const NightDef def = Quiet();
    NightState state = Built(def, 4);
    state.marked = 6;
    PassToMidnight(def, state);
    const int taken = TakenGuest(state);
    const int hp = state.guests[U(taken)].hp;
    // The house (traps, events) does not hurt the Taken.
    Hurt(def, state, taken, 10, -1);
    CHECK(state.guests[U(taken)].hp == hp);
    // A monster in the Taken's room does not fight it; walking a guest in, the monster is on the Taken's side.
    state.monsters.push_back({0, state.guests[U(taken)].cell, 99, 50, 2, 1});
    state.guests[U(taken)].cell = state.foyer;
    state.monsters.back().cell = state.foyer;
    while (Current(state) == taken)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    REQUIRE(Apply(def, state, {ActionKind::Attack, -1, I8(taken)}));
    bool monsterWithTaken = false;
    for (int f = 0; f < state.battle.count; ++f)
        monsterWithTaken = monsterWithTaken || (state.battle.fighters[U(f)].monster >= 0 && state.battle.fighters[U(f)].side == 1);
    CHECK(monsterWithTaken);
    CHECK(state.guests[U(taken)].hp <= hp);
}

TEST_CASE("The night ends three ways (T101)", "[night][hunt]")
{
    const NightDef def = Quiet();
    NightState win = Built(def, 4);
    win.marked = 6;
    PassToMidnight(def, win);
    Hurt(def, win, 6, 999, 0);
    CHECK(Result(win) == Outcome::Company);
    CHECK(Current(win) == -1);

    NightState lose = Built(def, 4);
    lose.marked = 6;
    PassToMidnight(def, lose);
    for (int g = 0; g < 6; ++g)
        Hurt(def, lose, g, 999, 6);
    CHECK(Result(lose) == Outcome::Taken);

    NightState late = Built(def, 4);
    late.marked = 6;
    while (Current(late) >= 0)
        REQUIRE(Apply(def, late, {ActionKind::EndTurn}));
    CHECK(Result(late) == Outcome::Taken);
}
