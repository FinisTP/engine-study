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

    // Every guest passes until the hunt begins (round turnAfterRound + 1). The failed-goal penalty
    // (1 HP, GoalTests) is undone so these tests see the hunt at the HP the evening left.
    void PassToMidnight(const NightDef &def, NightState &state)
    {
        std::array<int, maxGuests> hp{};
        for (int g = 0; g < state.count; ++g)
            hp[U(g)] = state.guests[U(g)].hp;
        while (Current(state) >= 0 && state.round <= def.rules.clock.turnAfterRound)
        {
            const auto legal = LegalActions(def, state);
            REQUIRE(Apply(def, state, legal[0].kind == ActionKind::LevelUp ? legal[0] : Action{ActionKind::EndTurn}));
        }
        for (int g = 0; g < state.count; ++g)
            if (!IsTaken(state, g) && !state.guests[U(g)].dead)
                state.guests[U(g)].hp = static_cast<std::int16_t>(std::max<int>(state.guests[U(g)].hp, hp[U(g)]));
    }

    // A board built far enough to hold the under-stage.
    NightState Built(const NightDef &def, std::uint64_t seed, int player = -1)
    {
        NightState state = FoyerNight(def, seed, player);
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
    // Not the guest whose turn it is: they are set down by hand.
    const int marked = state.order[1], trapped = state.order[2], struck = state.order[3];
    state.marked = I8(marked);
    state.guests[U(trapped)].down = true, state.guests[U(trapped)].hp = 0;
    state.guests[U(struck)].down = true, state.guests[U(struck)].hp = 0, state.guests[U(struck)].downedBy = state.order[0];
    state.guests[U(state.order[4])].cell = state.understage;
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

TEST_CASE("Chance picks the Taken among the living at midnight (T101)", "[night][hunt]")
{
    const NightDef def = Quiet();
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
    const int m = state.order[1];
    state.marked = I8(m);
    Hurt(def, state, m, 999, -1);
    REQUIRE(state.guests[U(m)].down);
    CHECK(Result(state) == Outcome::Running);
    PassToMidnight(def, state);
    CHECK(TakenGuest(state) == m);
    CHECK_FALSE(state.guests[U(m)].down);
    CHECK(state.guests[U(m)].hp == def.guests[U(m)].takenHp);
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

namespace
{
    constexpr std::uint8_t N = 1, E = 2, S = 4, W = 8;

    // A hunt where `form` is the Taken (guest 6's power is replaced), on a hand-built board:
    // row 0 is a corridor (0,0)-(4,0); (4,1) hangs below its end. Every guest is placed by hand.
    struct Hunt
    {
        NightDef def;
        NightState state;
        int taken = 6;
        int Cell(int x, int y) const { return CellAt(def, x, y); }
    };

    Hunt MakeHunt(const char *form, std::initializer_list<std::pair<int, std::uint8_t>> extra = {})
    {
        Hunt h{Quiet(), {}};
        h.def.guests[6].power = NightDef::Find(h.def.powers, form);
        for (GuestDef &g : h.def.guests)
            g.hp = 40;
        h.state = Built(h.def, 4);
        h.state.marked = 6;
        PassToMidnight(h.def, h.state);
        const int room = NightDef::Find(h.def.rooms, "auditorium");
        h.state.cells = {};
        auto open = [&](int cell, std::uint8_t doors) { h.state.cells[U(cell)] = {I8(room), doors, true}; };
        open(h.Cell(0, 0), E);
        open(h.Cell(1, 0), E | W);
        open(h.Cell(2, 0), E | W);
        open(h.Cell(3, 0), E | W);
        open(h.Cell(4, 0), W | S);
        open(h.Cell(4, 1), N);
        for (auto [cell, doors] : extra)
            open(cell, doors);
        // Park everyone far away in a closet (no path), then make it the Taken's turn.
        Closet(h.def, h.state, h.Cell(0, 4));
        for (Guest &g : h.state.guests)
            g.cell = I8(h.Cell(0, 4));
        while (Current(h.state) != 6)
            REQUIRE(Apply(h.def, h.state, {ActionKind::EndTurn}));
        h.state.guests[6].cell = I8(h.Cell(0, 0));
        return h;
    }

    const PowerDef &Power(const Hunt &h) { return h.def.powers[U(h.def.guests[6].power)]; }

    bool Offers(const Hunt &h, const Action &action)
    {
        const auto legal = LegalActions(h.def, h.state);
        return std::find(legal.begin(), legal.end(), action) != legal.end();
    }
}

TEST_CASE("Power: line (T102)", "[night][power]")
{
    Hunt h = MakeHunt("line");
    h.state.guests[0].cell = I8(h.Cell(3, 0));
    h.state.guests[1].cell = I8(h.Cell(4, 1));
    CHECK(Offers(h, {ActionKind::Power, 0}));
    CHECK_FALSE(Offers(h, {ActionKind::Power, 1}));
    const int hp = h.state.guests[0].hp;
    REQUIRE(Apply(h.def, h.state, {ActionKind::Power, 0}));
    CHECK(h.state.guests[0].hp == hp - Power(h).damage);
    CHECK_FALSE(h.state.battle.active);
}

TEST_CASE("Power: area (T102)", "[night][power]")
{
    Hunt h = MakeHunt("area");
    CHECK_FALSE(Offers(h, {ActionKind::Power}));
    h.state.guests[0].cell = I8(h.Cell(2, 0));
    h.state.guests[1].cell = I8(h.Cell(3, 0));
    REQUIRE(Offers(h, {ActionKind::Power}));
    const int hp0 = h.state.guests[0].hp, hp1 = h.state.guests[1].hp;
    REQUIRE(Apply(h.def, h.state, {ActionKind::Power}));
    CHECK(h.state.guests[0].hp == hp0 - Power(h).damage);
    CHECK(h.state.guests[1].hp == hp1);
}

TEST_CASE("Power: flood (T102)", "[night][power]")
{
    Hunt h = MakeHunt("flood");
    h.state.guests[6].cell = I8(h.Cell(1, 0));
    REQUIRE(Offers(h, {ActionKind::Power}));
    REQUIRE(Apply(h.def, h.state, {ActionKind::Power}));
    REQUIRE(Apply(h.def, h.state, {ActionKind::EndTurn}));
    const int wet = Current(h.state);
    REQUIRE(wet >= 0);
    h.state.guests[U(wet)].cell = I8(h.Cell(2, 0));
    CHECK(Stat(h.def, h.state, wet, StatKind::Speed) == 1);
    const int hp = h.state.guests[U(wet)].hp;
    h.state.guests[U(wet)].movesLeft = 0;
    REQUIRE(Apply(h.def, h.state, {ActionKind::EndTurn}));
    CHECK(h.state.guests[U(wet)].hp == hp - Power(h).damage);
    const int dry = Current(h.state);
    h.state.guests[U(dry)].cell = I8(h.Cell(3, 0));
    CHECK(Stat(h.def, h.state, dry, StatKind::Speed) == h.def.guests[U(dry)].speed);
}

TEST_CASE("Power: pull (T102)", "[night][power]")
{
    Hunt h = MakeHunt("pull");
    const int range = Power(h).range;
    REQUIRE(range < 4);
    h.state.guests[0].cell = I8(h.Cell(range, 0));
    h.state.guests[1].cell = I8(h.Cell(range + 1, 0));
    CHECK(Offers(h, {ActionKind::Power, 0}));
    CHECK_FALSE(Offers(h, {ActionKind::Power, 1}));
    const int hp = h.state.guests[0].hp;
    REQUIRE(Apply(h.def, h.state, {ActionKind::Power, 0}));
    CHECK(h.state.guests[0].cell == h.Cell(0, 0));
    CHECK(h.state.guests[0].hp <= hp - Power(h).damage);
    CHECK(h.state.battle.active);
}

TEST_CASE("Power: seal (T102)", "[night][power]")
{
    // A loop below the corridor: (1,0)-(1,1)-(2,1)-(2,0), so one passage of the loop may be sealed.
    Hunt h = MakeHunt("seal", {{CellAt(Data(), 1, 1), N | E}, {CellAt(Data(), 2, 1), N | W}});
    h.state.cells[U(h.Cell(1, 0))].doors |= S;
    h.state.cells[U(h.Cell(2, 0))].doors |= S;
    h.state.guests[0].cell = I8(h.Cell(3, 0));
    const Action loop{ActionKind::Power, I8(h.Cell(1, 0)), 1}; // (1,0) east: the loop goes round it
    const Action cut{ActionKind::Power, I8(h.Cell(2, 0)), 1};  // (2,0) east: the only way to guest 0
    CHECK(Offers(h, loop));
    CHECK_FALSE(Offers(h, cut));
    REQUIRE(Apply(h.def, h.state, loop));
    CHECK_FALSE(Passage(h.def, h.state, h.Cell(1, 0), 1));
    CHECK_FALSE(Passage(h.def, h.state, h.Cell(2, 0), 3));
    CHECK(Steps(h.def, h.state, h.Cell(3, 0), h.Cell(0, 0)) == 5);
    // It opens again after its rounds.
    for (int i = 0; i < 7 * (Power(h).rounds + 1) && Current(h.state) >= 0; ++i)
    {
        if (h.state.battle.active)
            break;
        REQUIRE(Apply(h.def, h.state, {ActionKind::EndTurn}));
    }
    CHECK(Passage(h.def, h.state, h.Cell(1, 0), 1));
}

TEST_CASE("Power: swap (T102)", "[night][power]")
{
    Hunt h = MakeHunt("swap");
    h.state.guests[0].cell = I8(h.Cell(4, 1));
    REQUIRE(Offers(h, {ActionKind::Power, 0}));
    const int moves = h.state.guests[6].movesLeft;
    REQUIRE(Apply(h.def, h.state, {ActionKind::Power, 0}));
    CHECK(h.state.guests[6].cell == h.Cell(4, 1));
    CHECK(h.state.guests[0].cell == h.Cell(0, 0));
    CHECK(Current(h.state) == 6); // the Taken may still move and act
    CHECK(h.state.guests[6].movesLeft == moves);
}

TEST_CASE("Power: reflect (T102)", "[night][power]")
{
    Hunt h = MakeHunt("reflect");
    REQUIRE(Offers(h, {ActionKind::Power}));
    REQUIRE(Apply(h.def, h.state, {ActionKind::Power}));
    CHECK(h.state.taken.reflect);
    REQUIRE(Apply(h.def, h.state, {ActionKind::EndTurn}));
    const int g = Current(h.state);
    h.state.guests[U(g)].cell = h.state.guests[6].cell;
    const int takenHp = h.state.guests[6].hp, hp = h.state.guests[U(g)].hp;
    REQUIRE(Apply(h.def, h.state, {ActionKind::Attack, -1, 6}));
    REQUIRE(Current(h.state) == g);
    const auto legal = LegalActions(h.def, h.state);
    REQUIRE(Apply(h.def, h.state, *std::find_if(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::Strike; })));
    CHECK(h.state.guests[U(g)].hp < hp);
    CHECK(h.state.guests[6].hp == takenHp);
    CHECK_FALSE(h.state.taken.reflect);
}

TEST_CASE("A power is offered only with a valid target and respects its cooldown (T102)", "[night][power]")
{
    Hunt h = MakeHunt("line");
    CHECK_FALSE(Offers(h, {ActionKind::Power, 0}));
    h.def.powers[U(h.def.guests[6].power)].cooldown = 2;
    h.state.guests[0].cell = I8(h.Cell(3, 0));
    int uses = 0;
    for (int turn = 0; turn < 4; ++turn)
    {
        while (Current(h.state) != 6)
            REQUIRE(Apply(h.def, h.state, {ActionKind::EndTurn}));
        h.state.guests[6].cell = I8(h.Cell(0, 0));
        h.state.guests[0].cell = I8(h.Cell(3, 0));
        h.state.guests[0].hp = 40;
        if (Offers(h, {ActionKind::Power, 0}))
        {
            REQUIRE(Apply(h.def, h.state, {ActionKind::Power, 0}));
            CHECK_FALSE(Offers(h, {ActionKind::Power, 0})); // once per turn
            ++uses;
        }
        REQUIRE(Apply(h.def, h.state, {ActionKind::EndTurn}));
    }
    CHECK(uses == 2);
}

TEST_CASE("Changing a power's numbers in the data changes the game, not the code (T102)", "[night][power]")
{
    Hunt h = MakeHunt("area");
    h.def.powers[U(h.def.guests[6].power)].damage = 9;
    h.state.guests[0].cell = I8(h.Cell(1, 0));
    const int hp = h.state.guests[0].hp;
    REQUIRE(Apply(h.def, h.state, {ActionKind::Power}));
    CHECK(h.state.guests[0].hp == hp - 9);
}

TEST_CASE("The under-stage is on the board at the Turn even if nobody found it (T101)", "[night][hunt]")
{
    const NightDef def = Quiet();
    for (std::uint64_t seed = 1; seed <= 30; ++seed)
    {
        NightState state = FoyerNight(def, seed); // nobody opens a room all evening
        state.marked = 1;
        PassToMidnight(def, state);
        REQUIRE(state.understage >= 0);
        CHECK(state.guests[1].cell == state.understage);
        CHECK(Steps(def, state, state.foyer, state.understage) >= 1);
    }
}

TEST_CASE("At midnight the Taken's minions wait far from the foyer; ordinary monsters are gone (T101)", "[night][hunt]")
{
    NightDef def = Data();
    def.rules.monsters.chancePercent = 100;
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        NightState state = Built(def, seed);
        state.marked = 2;
        PassToMidnight(def, state);
        int minions = 0, expected = 0;
        for (const MonsterDef &m : def.monsters)
            expected += m.taken == 2 ? m.count : 0;
        for (const Monster &m : state.monsters)
        {
            const MonsterDef &md = def.monsters[U(m.kind)];
            CHECK(md.taken == 2); // only this form's minions, no ordinary monster
            CHECK(m.cell != state.foyer);
            CHECK(Steps(def, state, state.foyer, m.cell) >= 2);
            ++minions;
        }
        CHECK(minions == expected);
        // No ordinary monster appears in the hunt.
        for (int i = 0; i < 30 && Current(state) >= 0; ++i)
        {
            if (state.battle.active)
                FightOut(def, state);
            else
                REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
            for (const Monster &m : state.monsters)
                CHECK(def.monsters[U(m.kind)].taken == 2);
        }
    }
}

TEST_CASE("Killing a minion takes its share of the Taken's life (T101)", "[night][hunt]")
{
    const NightDef def = Quiet();
    NightState state = Built(def, 4);
    state.marked = 6;
    PassToMidnight(def, state);
    REQUIRE_FALSE(state.monsters.empty());
    const int taken = TakenGuest(state), hp = state.guests[U(taken)].hp;
    const MonsterDef &md = def.monsters[U(state.monsters[0].kind)];
    while (Current(state) == taken)
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    const int g = Current(state);
    state.monsters[0].hp = 1;
    state.monsters[0].cell = state.guests[U(g)].cell;
    REQUIRE(Apply(def, state, {ActionKind::Attack, 0}));
    FightOut(def, state);
    CHECK(state.guests[U(taken)].hp == hp - def.guests[U(taken)].takenHp * md.hpShare / 100);
}
