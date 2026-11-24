#include "MatchTestData.hpp"

#include <Match.hpp>

#include <algorithm>

// The campaign's fight rules on the faction match's engine (campaign plan 2, Match.md "Modes").
using namespace umbra::match;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }
    std::int8_t I8(int i) { return static_cast<std::int8_t>(i); }

    // A free open square next to `cell`, or -1.
    int Beside(const MatchState &s, int cell)
    {
        for (int d = 0; d < 4; ++d)
            if (const int n = Neighbour(s.board, cell, d); n >= 0 && std::ranges::none_of(s.units, [&](const Unit &u) { return Up(u) && u.cell == n; }))
                return n;
        return -1;
    }

    // End turns until `unit` is the one to act (others just pass).
    void TurnOf(const MatchDef &def, MatchState &s, int unit)
    {
        for (int i = 0; i < 200 && Current(s) != unit && Current(s) >= 0; ++i)
            REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
        REQUIRE(Current(s) == unit);
    }

    // `attacker` attacks `target` (moved beside it first); every pick is the first ability until the battle ends.
    void Fight(const MatchDef &def, MatchState &s, int attacker, int target)
    {
        TurnOf(def, s, attacker);
        Put(s, attacker, Beside(s, s.units[U(target)].cell));
        s.now.origin = s.units[U(attacker)].cell;
        REQUIRE(Apply(def, s, {ActionKind::Attack, I8(target)}));
        for (int i = 0; i < 50 && s.battle.on && !s.over; ++i)
            REQUIRE(Apply(def, s, {ActionKind::BattleAbility, 0}));
    }

    // The first character of each side in a faction match.
    std::pair<int, int> Foes(const MatchState &s)
    {
        int a = -1, b = -1;
        for (int i = static_cast<int>(s.units.size()) - 1; i >= 0; --i)
            if (s.units[U(i)].kind == 0)
                (s.units[U(i)].faction == 0 ? a : b) = i;
        return {a, b};
    }
}

TEST_CASE("Each switch does its part, and the faction match keeps them all (campaign plan 2)", "[match][fight]")
{
    const MatchDef def = Data();
    SECTION("essence and fight XP: off in the campaign, on in the faction match")
    {
        MatchState c = ::Fight(def);
        c.units[2].hp = 1;
        c.units[2].essence = 3;
        Fight(def, c, 0, 2);
        CHECK(c.units[0].essence == 0);
        CHECK(c.units[0].xp == 0);
        CHECK(c.floor.empty());

        MatchState f = NewMatch(def, 7, {0, 1, 2});
        const auto [mine, theirs] = Foes(f);
        f.units[U(theirs)].hp = 1;
        f.units[U(theirs)].essence = 3;
        const int before = f.units[U(mine)].xp;
        Fight(def, f, mine, theirs);
        CHECK(f.units[U(mine)].essence >= 3);
        CHECK(f.units[U(mine)].xp > before);
    }
    SECTION("perform: refused in the campaign")
    {
        MatchState c = ::Fight(def);
        TurnOf(def, c, 0);
        Put(c, 0, c.stage);
        c.units[0].essence = 2;
        CHECK(Check(def, c, {ActionKind::Perform}) == Why::Off);
    }
    SECTION("respawn: a downed unit never comes back in the campaign; in the faction match it counts down")
    {
        MatchState c = ::Fight(def);
        c.mode.fallsStay = false; // the down itself (falls are their own switch)
        c.units[1].hp = 1;
        Fight(def, c, 2, 1);
        CHECK(c.units[1].back == 0);
        MatchState f = NewMatch(def, 7, {0, 1, 2});
        const auto [mine, theirs] = Foes(f);
        f.units[U(mine)].hp = 1;
        Fight(def, f, theirs, mine);
        CHECK(f.units[U(mine)].back == def.rules.respawn.turns);
    }
    SECTION("respawn: no healing on one's home square in the campaign")
    {
        MatchState c = ::Fight(def);
        TurnOf(def, c, 0);
        c.units[0].home = c.units[0].cell;
        c.units[0].hp = 5;
        REQUIRE(Apply(def, c, {ActionKind::EndTurn}));
        CHECK(c.units[0].hp == 5);
    }
    SECTION("round limit: none in the campaign")
    {
        MatchState c = ::Fight(def);
        c.round = 40;
        for (int i = 0; i < 40 && c.round == 40; ++i)
            REQUIRE(Apply(def, c, {ActionKind::EndTurn}));
        CHECK(c.round == 41);
        CHECK_FALSE(c.over);
        CHECK(TurnsLeft(def, c) == -1);
    }
}

namespace
{
    // The campaign's ability and item kinds, added to a copy of the data (plan 4 gives the kits).
    int AddAbility(MatchDef &def, const char *id, AbilityClass kind, Target target, int power, int range = 3)
    {
        AbilityDef a;
        a.id = a.name = id;
        a.use = Use::Board;
        a.kind = kind;
        a.target = target;
        a.power = power;
        a.range = range;
        def.abilities.push_back(a);
        return static_cast<int>(def.abilities.size()) - 1;
    }

    void Give(MatchState &s, int unit, int ability) { s.units[U(unit)].abilities[maxAbilities - 1] = I8(ability); }

    void Fall(MatchState &s, int unit)
    {
        s.units[U(unit)].hp = 0;
        s.units[U(unit)].ap = 0;
    }
}

TEST_CASE("A fallen unit stays on its square, can't be hit, is skipped and doesn't block (campaign plan 2)", "[match][fight][falls]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.units[2].flags = 0; // not the boss here, so its fall doesn't end the fight
    s.units[2].hp = 1;
    Fight(def, s, 0, 2); // the player strikes the boss down in a battle
    const int square = s.units[2].cell;
    CHECK_FALSE(s.battle.on); // the battle is over: no one of the boss's group stands
    CHECK(square >= 0);
    CHECK(Fallen(s.units[2]));
    s.now.acted = false;
    CHECK(Check(def, s, {ActionKind::Attack, 2}) == Why::Fallen);
    // Two of the player's side may stand on its square; it doesn't count.
    Put(s, 0, square);
    Put(s, 1, square);
    CHECK(Up(s.units[0]));
    // A whole round passes without the boss's turn.
    const int round = s.round;
    std::vector<int> seen;
    for (int i = 0; i < 50 && s.round <= round + 1; ++i)
    {
        seen.push_back(Current(s));
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    }
    CHECK(std::ranges::count(seen, 2) == 0);
}

TEST_CASE("AP is lost on a fall (campaign plan 2)", "[match][fight][falls]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.units[1].ap = 2;
    s.units[1].hp = 1;
    Fight(def, s, 2, 1);
    REQUIRE(Fallen(s.units[1]));
    CHECK(s.units[1].ap == 0);
}

TEST_CASE("Revive stands a fallen ally up with part of its HP, on the nearest free square when its own is full (campaign plan 2)", "[match][fight][revive]")
{
    MatchDef def = Data();
    const int revive = AddAbility(def, "revive", AbilityClass::Revive, Target::Ally, 30);
    MatchState s = ::Fight(def);
    Give(s, 0, revive);
    // 1. The ally has fallen; two more of the player's side stand on its square.
    Fall(s, 1);
    const int square = s.units[1].cell;
    for (int i = 0; i < 2; ++i)
    {
        Unit u = s.units[1];
        u.hp = 10, u.flags = 0;
        s.units.push_back(u);
    }
    TurnOf(def, s, 0);
    Put(s, 0, Beside(s, square));
    s.now.origin = s.units[0].cell;
    // 2. The nearest free square, ties by distance from the reviver, then the lower cell.
    int expect = -1, best = 1 << 20, tie = 1 << 20;
    for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
    {
        const int d = Steps(s.board, square, c);
        // Room for one more of side 0: no foe standing there, fewer than two (one on the stage).
        const auto there = std::ranges::count_if(s.units, [&](const Unit &o) { return Up(o) && o.cell == c; });
        const bool foe = std::ranges::any_of(s.units, [&](const Unit &o) { return Up(o) && o.cell == c && o.faction != 0; });
        if (d <= 0 || foe || there >= (c == s.stage ? 1 : def.rules.board.squareCapacity))
            continue;
        const int fromMe = Steps(s.board, s.units[0].cell, c);
        if (d < best || (d == best && fromMe < tie))
            expect = c, best = d, tie = fromMe;
    }
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 1}));
    CHECK(s.units[1].hp == s.units[1].maxHp * 30 / 100);
    CHECK(s.units[1].cell == expect);
    CHECK(s.units[1].from == s.round + 1); // it acts from the next round
}

TEST_CASE("A revive item works like the ability (campaign plan 2)", "[match][fight][revive]")
{
    MatchDef def = Data();
    ItemDef rice;
    rice.id = rice.name = "rice";
    rice.kind = ItemClass::Revive;
    rice.amount = 50;
    def.items.push_back(rice);
    MatchState s = ::Fight(def);
    Fall(s, 1);
    TurnOf(def, s, 0);
    Put(s, 0, Beside(s, s.units[1].cell));
    s.units[0].items[0] = I8(static_cast<int>(def.items.size()) - 1);
    CHECK(Check(def, s, {ActionKind::UseItem, 0}) == Why::NothingHere); // a revive item needs a target
    CHECK(Check(def, s, {ActionKind::UseItem, 0, 2}) == Why::NotFallen); // the boss stands
    REQUIRE(Apply(def, s, {ActionKind::UseItem, 0, 1}));
    CHECK(s.units[1].hp == s.units[1].maxHp / 2);
    CHECK(s.units[0].items[0] == -1);
}

TEST_CASE("Revive and remove need a fallen target; a removed unit is gone for the fight (campaign plan 2)", "[match][fight][revive]")
{
    MatchDef def = Data();
    const int revive = AddAbility(def, "revive", AbilityClass::Revive, Target::Ally, 30);
    const int remove = AddAbility(def, "remove", AbilityClass::Remove, Target::Enemy, 0);
    MatchState s = ::Fight(def);
    s.units[2].flags = 0; // not the boss here, so its removal doesn't end the fight
    TurnOf(def, s, 0);
    Put(s, 0, Beside(s, s.units[2].cell));
    s.now.origin = s.units[0].cell;
    Give(s, 0, remove);
    CHECK(Check(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 2}) == Why::NotFallen);
    Fall(s, 2);
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 2}));
    CHECK(s.units[2].cell == -1);
    CHECK((s.units[2].flags & flag::Removed) != 0);
    // Nobody can stand it up again.
    s.units[3].abilities[maxAbilities - 1] = I8(revive);
    TurnOf(def, s, 3);
    s.units[3].hp = 5;
    CHECK(Check(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 2}) == Why::NotFallen);
}

TEST_CASE("Enemies revive their own; a strung recruit stands up strung (campaign plan 2)", "[match][fight][revive]")
{
    MatchDef def = Data();
    const int revive = AddAbility(def, "revive", AbilityClass::Revive, Target::Ally, 30);
    MatchState s = ::Fight(def);
    Give(s, 2, revive);
    Fall(s, 3);
    TurnOf(def, s, 2);
    Put(s, 2, Beside(s, s.units[3].cell));
    s.now.origin = s.units[2].cell;
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 3}));
    CHECK(Up(s.units[3]));
    CHECK((s.units[3].flags & flag::Strung) != 0);
}

namespace
{
    // An action that spends the turn without a battle: drinking a bandage.
    void UseBandage(const MatchDef &def, MatchState &s, int unit, int slot)
    {
        s.units[U(unit)].items[U(slot)] = I8(MatchDef::Find(def.items, "bandage"));
        REQUIRE(Apply(def, s, {ActionKind::UseItem, I8(slot)}));
    }

    int OpenMove(const MatchDef &def, const MatchState &s)
    {
        for (int d = 0; d < 4; ++d)
            if (Check(def, s, {ActionKind::Move, I8(d)}) == Why::Ok)
                return d;
        return -1;
    }
}

TEST_CASE("Biding is the whole turn and gives 1 AP, at most 2 (campaign plan 2)", "[match][fight][biding]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.mode.biding = true; // off in the data for now (the user, 2026-11-24); its rules stay
    for (int k = 1; k <= 3; ++k)
    {
        TurnOf(def, s, 0);
        REQUIRE(Apply(def, s, {ActionKind::Bide}));
        CHECK(Current(s) != 0); // the turn is over
        CHECK(s.units[0].ap == std::min(k, 2));
    }
}

TEST_CASE("A bide is refused after a move, after an action, and in the faction match (campaign plan 2)", "[match][fight][biding]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.mode.biding = true; // off in the data for now (the user, 2026-11-24); its rules stay
    TurnOf(def, s, 0);
    MatchState moved = s;
    const int d = OpenMove(def, moved);
    REQUIRE(d >= 0);
    REQUIRE(Apply(def, moved, {ActionKind::Move, I8(d)}));
    CHECK(Check(def, moved, {ActionKind::Bide}) == Why::AlreadyActed);
    MatchState acted = s;
    UseBandage(def, acted, 0, 0);
    CHECK(Check(def, acted, {ActionKind::Bide}) == Why::AlreadyActed);
    const MatchState f = NewMatch(def, 7, {0, 1, 2});
    CHECK(Check(def, f, {ActionKind::Bide}) == Why::Off);
    CHECK(Check(def, f, {ActionKind::Extra}) == Why::Off);
}

TEST_CASE("With 2 AP a turn has three actions, each with its own move (campaign plan 2)", "[match][fight][biding]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.mode.biding = true; // off in the data for now (the user, 2026-11-24); its rules stay
    TurnOf(def, s, 0);
    s.units[0].ap = 2;
    s.units[0].hp = 1;
    CHECK(Check(def, s, {ActionKind::Extra}) == Why::NothingHere); // act first
    UseBandage(def, s, 0, 0);
    REQUIRE(Apply(def, s, {ActionKind::Extra}));
    CHECK(s.now.movesLeft == s.units[0].move);
    const int d = OpenMove(def, s);
    REQUIRE(d >= 0);
    REQUIRE(Apply(def, s, {ActionKind::Move, I8(d)}));
    UseBandage(def, s, 0, 1);
    REQUIRE(Apply(def, s, {ActionKind::Extra}));
    UseBandage(def, s, 0, 2);
    CHECK(s.units[0].ap == 0);
    CHECK(Check(def, s, {ActionKind::Extra}) == Why::NoAp);
    CHECK(Current(s) == 0); // still the player's turn until it ends it
}

TEST_CASE("An AP ability is paid first from the same pool and limit (campaign plan 2)", "[match][fight][biding]")
{
    MatchDef def = Data();
    const int one = AddAbility(def, "rally1", AbilityClass::Heal, Target::Ally, 3);
    const int two = AddAbility(def, "rally2", AbilityClass::Heal, Target::Ally, 3);
    def.abilities[U(one)].ap = 1;
    def.abilities[U(two)].ap = 2;
    MatchState s = ::Fight(def);
    s.mode.biding = true; // off in the data for now (the user, 2026-11-24); its rules stay
    TurnOf(def, s, 0);
    s.units[0].hp = 5;
    s.units[0].abilities[6] = I8(one);
    s.units[0].abilities[7] = I8(two);
    const Action heal1{ActionKind::BoardAbility, 6, 0}, heal2{ActionKind::BoardAbility, 7, 0};
    // 1. No AP: both are refused and Legal leaves them out.
    CHECK(Check(def, s, heal1) == Why::NoAp);
    const std::vector<Action> legal = Legal(def, s);
    CHECK(std::ranges::find(legal, heal1) == legal.end());
    // 2. Two AP: the 1-AP ability, then one extra action, then no more.
    MatchState a = s;
    a.units[0].ap = 2;
    REQUIRE(Apply(def, a, heal1));
    CHECK(a.units[0].ap == 1);
    REQUIRE(Apply(def, a, {ActionKind::Extra}));
    UseBandage(def, a, 0, 0);
    CHECK(Check(def, a, {ActionKind::Extra}) == Why::NoAp);
    // 3. Two AP: the 2-AP ability alone.
    MatchState b = s;
    b.units[0].ap = 2;
    REQUIRE(Apply(def, b, heal2));
    CHECK(b.units[0].ap == 0);
    CHECK(Check(def, b, {ActionKind::Extra}) == Why::NoAp);
}

TEST_CASE("Give-AP moves one AP to an ally (campaign plan 2)", "[match][fight][biding]")
{
    MatchDef def = Data();
    const int give = AddAbility(def, "word", AbilityClass::GiveAp, Target::Ally, 0, 6);
    MatchState s = ::Fight(def);
    s.mode.biding = true; // off in the data for now (the user, 2026-11-24); its rules stay
    TurnOf(def, s, 0);
    Give(s, 0, give);
    const Action pass{ActionKind::BoardAbility, maxAbilities - 1, 1};
    CHECK(Check(def, s, pass) == Why::NoAp); // nothing to give
    s.units[0].ap = 1;
    s.units[1].ap = 2;
    CHECK(Check(def, s, pass) == Why::NoAp); // the ally is full
    s.units[1].ap = 0;
    Put(s, 1, Beside(s, s.units[0].cell));
    REQUIRE(Apply(def, s, pass));
    CHECK(s.units[0].ap == 0);
    CHECK(s.units[1].ap == 1);
}

TEST_CASE("Every unit starts a fight at 0 AP (campaign plan 2)", "[match][fight][biding]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.mode.biding = true; // off in the data for now (the user, 2026-11-24); its rules stay
    for (Unit &u : s.units)
        u.ap = 2;
    BeginMatch(def, s, 9);
    for (const Unit &u : s.units)
        CHECK(u.ap == 0);
}

TEST_CASE("Only the player cuts, beside a fallen strung recruit, as the turn's action (campaign plan 2)", "[match][fight][free]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    const Action cut{ActionKind::Cut, 3};
    TurnOf(def, s, 1);
    Put(s, 1, Beside(s, s.units[3].cell));
    CHECK(Check(def, s, cut) == Why::OnlyPlayer);
    TurnOf(def, s, 0);
    CHECK(Check(def, s, cut) == Why::NotFallen); // still standing
    Fall(s, 3);
    CHECK(Check(def, s, cut) == Why::OutOfRange);
    Put(s, 0, Beside(s, s.units[3].cell));
    Fall(s, 2);
    Put(s, 2, Beside(s, s.units[0].cell));
    CHECK(Check(def, s, {ActionKind::Cut, 2}) == Why::NotStrung); // the boss has no gold knot
    s.units[2].hp = 5;
    const int square = s.units[3].cell;
    REQUIRE(Apply(def, s, cut));
    CHECK((s.units[3].flags & flag::Freed) != 0);
    CHECK((s.units[3].flags & flag::Strung) == 0);
    CHECK(s.units[3].cell == -1); // gone from the board with a gold light (the user, 2026-11-23)
    CHECK(s.log.back().kind == LogKind::Freed);
    CHECK(s.log.back().c == square);
    CHECK(s.now.acted);
}

TEST_CASE("The fight is won when the boss falls and, if the stage asks, the recruit is freed (campaign plan 2)", "[match][fight][end]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.mode.mustFree = true;
    Fall(s, 3);
    TurnOf(def, s, 0);
    Put(s, 0, Beside(s, s.units[3].cell));
    REQUIRE(Apply(def, s, {ActionKind::Cut, 3}));
    CHECK_FALSE(s.over);
    REQUIRE(Apply(def, s, {ActionKind::EndTurn})); // the cut was this turn's action
    s.units[2].hp = 1;
    Fight(def, s, 0, 2);
    CHECK(s.over);
    CHECK(s.winner == 0);
    CHECK(s.loss == Loss::None);

    MatchState t = ::Fight(def); // without the extra condition, the boss alone
    t.units[2].hp = 1;
    Fight(def, t, 0, 2);
    CHECK(t.over);
    CHECK(t.winner == 0);
}

TEST_CASE("The boss falling while the recruit is strung loses the fight (campaign plan 2)", "[match][fight][end]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.mode.mustFree = true;
    s.units[2].hp = 1;
    Fight(def, s, 0, 2);
    CHECK(s.over);
    CHECK(s.winner == 1);
    CHECK(s.loss == Loss::Trapped);
}

TEST_CASE("A removed strung recruit loses a fight that must free it (campaign plan 2)", "[match][fight][end]")
{
    MatchDef def = Data();
    const int remove = AddAbility(def, "remove", AbilityClass::Remove, Target::Enemy, 0);
    MatchState s = ::Fight(def);
    s.mode.mustFree = true;
    Fall(s, 3);
    TurnOf(def, s, 0);
    Give(s, 0, remove);
    Put(s, 0, Beside(s, s.units[3].cell));
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 3}));
    CHECK(s.over);
    CHECK(s.loss == Loss::Trapped);
}

TEST_CASE("The player falling ends the fight at once, even in an enemy's battle (campaign plan 2)", "[match][fight][end]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.units[0].hp = 1;
    TurnOf(def, s, 2);
    Put(s, 2, Beside(s, s.units[0].cell));
    s.now.origin = s.units[2].cell;
    REQUIRE(Apply(def, s, {ActionKind::Attack, 0}));
    for (int i = 0; i < 50 && s.battle.on && !s.over; ++i)
        REQUIRE(Apply(def, s, {ActionKind::BattleAbility, 0}));
    CHECK(s.over);
    CHECK(s.winner == 1);
    CHECK(s.loss == Loss::PlayerFell);
    CHECK(Current(s) == -1); // no rite offered: not ready
}

TEST_CASE("The whole side down is a loss (campaign plan 2)", "[match][fight][end]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.mode.playerLoss = false; // so the side, not the player, decides
    Fall(s, 0);
    s.units[1].hp = 1;
    Fight(def, s, 2, 1);
    CHECK(s.over);
    CHECK(s.loss == Loss::SideDown);
}

TEST_CASE("The rite stands the side up and the fight goes on; once used it is gone, even after another loss (campaign plan 2)", "[match][fight][rite]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.riteReady = true;
    Fall(s, 1);
    s.units[0].hp = 1;
    Fight(def, s, 2, 0);
    REQUIRE(s.over);
    REQUIRE(s.loss == Loss::PlayerFell);
    CHECK(Current(s) == 0); // the rite is offered to the player
    CHECK(Check(def, s, {ActionKind::EndTurn}) == Why::Over);
    REQUIRE(Apply(def, s, {ActionKind::Rite}));
    CHECK_FALSE(s.over);
    CHECK(s.loss == Loss::None);
    CHECK(s.riteUsed);
    CHECK_FALSE(s.riteReady);
    CHECK(s.units[0].hp == s.units[0].maxHp);
    CHECK(s.units[1].hp == s.units[1].maxHp);
    CHECK_FALSE(s.battle.on);
    CHECK(Current(s) >= 0);
    // A second loss: no rite any more.
    s.units[0].hp = 1;
    Fight(def, s, 2, 0);
    REQUIRE(s.over);
    CHECK(Check(def, s, {ActionKind::Rite}) == Why::NoRite);
    CHECK(s.riteUsed);
}

TEST_CASE("No rite for a trapped loss or when not ready (campaign plan 2)", "[match][fight][rite]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.riteReady = true;
    s.mode.mustFree = true;
    s.units[2].hp = 1;
    Fight(def, s, 0, 2);
    REQUIRE(s.loss == Loss::Trapped);
    CHECK(Check(def, s, {ActionKind::Rite}) == Why::NoRite);
    CHECK(Current(s) == -1);
}

TEST_CASE("Biding is off in the campaign's data for now (the user, 2026-11-24)", "[match][fight][biding]")
{
    const MatchDef def = Data();
    CHECK_FALSE(def.modes.at("campaign").biding);
    MatchState s = ::Fight(def);
    TurnOf(def, s, 0);
    CHECK(Check(def, s, {ActionKind::Bide}) == Why::Off);
}

TEST_CASE("Hasten: the ally takes a full extra turn right after the user and keeps its own (campaign plan 4)", "[match][fight][hasten]")
{
    MatchDef def = Data();
    const int word = AddAbility(def, "word", AbilityClass::Hasten, Target::Ally, 0, 6);
    MatchState s = ::Fight(def);
    TurnOf(def, s, 0);
    Give(s, 0, word);
    Put(s, 1, Beside(s, s.units[0].cell));
    const Action onSelf{ActionKind::BoardAbility, maxAbilities - 1, 0}, onAlly{ActionKind::BoardAbility, maxAbilities - 1, 1};
    CHECK(Check(def, s, onSelf) != Why::Ok);
    const auto count = [&] { return std::ranges::count(s.order, I8(1)); };
    REQUIRE(count() == 1);
    REQUIRE(Apply(def, s, onAlly));
    CHECK(Check(def, s, onAlly) == Why::AlreadyActed); // once a turn
    CHECK(count() == 2);                                // its extra turn and its own
    REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    CHECK(Current(s) == 1);
    CHECK(s.now.movesLeft == s.units[1].move); // a whole turn: move and act
    // The extra place goes with the round: the next round has it once.
    const int round = s.round;
    for (int i = 0; i < 50 && s.round == round; ++i)
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    CHECK(count() == 1);
}

TEST_CASE("After hasten, a unit with the passive gets +3 move on its next turn only (campaign plan 4)", "[match][fight][hasten]")
{
    MatchDef def = Data();
    const int word = AddAbility(def, "word", AbilityClass::Hasten, Target::Ally, 0, 6);
    const int feet = AddAbility(def, "feet", AbilityClass::AfterHasten, Target::Self, 3);
    def.abilities[U(feet)].use = Use::Passive;
    MatchState s = ::Fight(def);
    TurnOf(def, s, 0);
    s.units[0].abilities[maxAbilities - 2] = I8(feet);
    Give(s, 0, word);
    Put(s, 1, Beside(s, s.units[0].cell));
    const int move = s.units[0].move;
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 1}));
    REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    TurnOf(def, s, 0);
    CHECK(s.now.movesLeft == move + 3);
    REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    TurnOf(def, s, 0);
    CHECK(s.now.movesLeft == move);
}

TEST_CASE("Items say where they can be used; a cafe item can't be used in a fight (campaign plan 4)", "[match][fight][items]")
{
    MatchDef def = Data();
    REQUIRE((def.items[0].use & useBoard) != 0); // the shipped items work on the board
    ItemDef jam = def.items[0];
    jam.id = jam.name = "jam";
    jam.use = useCafe;
    def.items.push_back(jam);
    MatchState s = ::Fight(def);
    TurnOf(def, s, 0);
    s.units[0].items[0] = I8(static_cast<int>(def.items.size()) - 1);
    CHECK(Check(def, s, {ActionKind::UseItem, 0}) == Why::NotHere);
}

TEST_CASE("Ingredients and keepsakes picked up go to the found list, never a slot (campaign plan 4)", "[match][fight][items]")
{
    MatchDef def = Data();
    ItemDef ginger = def.items[0];
    ginger.id = ginger.name = "ginger";
    ginger.family = ItemFamily::Ingredient;
    ginger.use = useCafe;
    def.items.push_back(ginger);
    const auto g = I8(static_cast<int>(def.items.size()) - 1);
    MatchState s = ::Fight(def);
    TurnOf(def, s, 0);
    s.units[0].items = {0, 0, 0, 0}; // a full bag
    s.floor.push_back({s.units[0].cell, g, 0, false});
    const Action pick{ActionKind::PickUp, I8(static_cast<int>(s.floor.size()) - 1)};
    REQUIRE(Check(def, s, pick) == Why::Ok);
    REQUIRE(Apply(def, s, pick));
    CHECK(s.found == std::vector<std::int8_t>{g});
    CHECK(std::ranges::count(s.units[0].items, g) == 0);
}

TEST_CASE("A unit's drops are rolled from the fight's seed when it falls (campaign plan 4)", "[match][fight][drops]")
{
    MatchDef def = Data();
    const int blow = AddAbility(def, "blow", AbilityClass::Hit, Target::Enemy, 99, 99);
    const auto falls = [&](std::uint64_t seed, int percent) {
        MatchState s = ::Fight(def, seed);
        s.units[2].flags = 0;
        s.drops.push_back({2, 0, I8(percent)});
        TurnOf(def, s, 0);
        Give(s, 0, blow);
        REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 2}));
        REQUIRE(s.units[2].hp <= 0);
        return s.found;
    };
    CHECK(falls(5, 100) == std::vector<std::int8_t>{0});
    CHECK(falls(5, 0).empty());
    CHECK(falls(9, 30) == falls(9, 30)); // the same seed, the same drop
    int got = 0;
    for (std::uint64_t seed = 1; seed <= 400; ++seed)
        got += falls(seed, 30).empty() ? 0 : 1;
    CHECK(got > 80);
    CHECK(got < 160);
}

TEST_CASE("A side marked noRevive can't revive (campaign plan 4)", "[match][fight][revive]")
{
    MatchDef def = Data();
    const int raise = AddAbility(def, "raise", AbilityClass::Revive, Target::Ally, 50);
    MatchState s = ::Fight(def);
    s.units[2].flags = 0;
    Fall(s, 3);
    TurnOf(def, s, 2);
    Give(s, 2, raise);
    Put(s, 2, Beside(s, s.units[3].cell));
    const Action revive{ActionKind::BoardAbility, maxAbilities - 1, 3};
    REQUIRE(Check(def, s, revive) == Why::Ok);
    s.noRevive = 2; // side 1
    CHECK(Check(def, s, revive) == Why::Off);
}

namespace
{
    // An ability with an effect (campaign plan 4, Match.md "Effects").
    int AddEffect(MatchDef &def, const char *id, Use use, AbilityClass kind, Target target, AbilityEffect effect, int amount, int power = 0, int range = 3)
    {
        const int a = AddAbility(def, id, kind, target, power, range);
        def.abilities[U(a)].use = use;
        def.abilities[U(a)].effect = effect;
        def.abilities[U(a)].amount = amount;
        return a;
    }
}

TEST_CASE("Mark: the target takes more damage until its next turn (campaign plan 4)", "[match][fight][effects]")
{
    MatchDef def = Data();
    const int mark = AddEffect(def, "diagnose", Use::Board, AbilityClass::Effect, Target::Enemy, AbilityEffect::Mark, 1, 0, 6);
    MatchState s = ::Fight(def);
    s.units[2].flags = 0;
    TurnOf(def, s, 0);
    Give(s, 0, mark);
    const int before = Damage(def, s, s.units[1].abilities[0], 1, 2, 0);
    Put(s, 0, Beside(s, s.units[2].cell));
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 2}));
    CHECK(s.units[2].marked == 1);
    CHECK(Damage(def, s, s.units[1].abilities[0], 1, 2, 0) == before + 1);
    TurnOf(def, s, 2);
    CHECK(s.units[2].marked == 0);
}

TEST_CASE("Alarm: the target loses Speed until its next turn and acts later in the next round's order (campaign plan 4)", "[match][fight][effects]")
{
    MatchDef def = Data();
    const int alarm = AddEffect(def, "alarm", Use::Board, AbilityClass::Effect, Target::Enemy, AbilityEffect::Alarm, 2, 0, 9);
    MatchState s = ::Fight(def);
    s.units[2].flags = 0;
    TurnOf(def, s, 0);
    Give(s, 0, alarm);
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 2}));
    CHECK(s.units[2].slow == 2);
    TurnOf(def, s, 2);
    CHECK(s.units[2].slow == 0);
}

TEST_CASE("First shot: more power against a unit that hasn't acted this round (campaign plan 4)", "[match][fight][effects]")
{
    MatchDef def = Data();
    const int first = AddEffect(def, "firstshot", Use::Battle, AbilityClass::Strike, Target::Enemy, AbilityEffect::FirstShot, 3, 3, 1);
    MatchState s = ::Fight(def);
    TurnOf(def, s, 0);
    s.order = {3, 0, 2, 1}; // the recruit has acted, the player acts now, the boss after
    s.turn = 1;
    const int plain = AddAbility(def, "plain", AbilityClass::Strike, Target::Enemy, 3, 1);
    def.abilities[U(plain)].use = Use::Battle;
    CHECK(Damage(def, s, first, 0, 2, 0) == Damage(def, s, plain, 0, 2, 0) + 3);
    CHECK(Damage(def, s, first, 0, 3, 0) == Damage(def, s, plain, 0, 3, 0));
}

TEST_CASE("Speedkill: double damage to a slower target only (campaign plan 4)", "[match][fight][effects]")
{
    MatchDef def = Data();
    const int kill = AddEffect(def, "speedkill", Use::Passive, AbilityClass::Passive, Target::Self, AbilityEffect::Speedkill, 2);
    MatchState s = ::Fight(def);
    const int hit = s.units[2].abilities[0];
    s.units[1].speed = 1, s.units[2].speed = 5;
    const int plain = Damage(def, s, hit, 2, 1, 0);
    s.units[2].abilities[maxAbilities - 1] = I8(kill);
    CHECK(Damage(def, s, hit, 2, 1, 0) == plain * 2);
    s.units[1].speed = 9; // faster now: no double
    CHECK(Damage(def, s, hit, 2, 1, 0) == plain);
}

TEST_CASE("Death paper: a kill heals its owner (campaign plan 4)", "[match][fight][effects]")
{
    MatchDef def = Data();
    const int paper = AddEffect(def, "paper", Use::Passive, AbilityClass::Passive, Target::Self, AbilityEffect::DeathPaper, 2);
    const int blow = AddAbility(def, "blow", AbilityClass::Hit, Target::Enemy, 99, 99);
    MatchState s = ::Fight(def);
    s.units[2].flags = 0;
    TurnOf(def, s, 0);
    s.units[0].abilities[maxAbilities - 2] = I8(paper);
    Give(s, 0, blow);
    s.units[0].hp = static_cast<std::int16_t>(s.units[0].maxHp - 5);
    const int hp = s.units[0].hp;
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 2}));
    CHECK(s.units[0].hp == hp + 2);
}

TEST_CASE("Cold rice: heals and clears a mark on one standing, revives one fallen (campaign plan 4)", "[match][fight][effects]")
{
    MatchDef def = Data();
    const int rice = AddEffect(def, "rice", Use::Board, AbilityClass::Revive, Target::Ally, AbilityEffect::Rice, 5, 30, 1);
    MatchState s = ::Fight(def);
    TurnOf(def, s, 0);
    Give(s, 0, rice);
    Put(s, 1, Beside(s, s.units[0].cell));
    s.units[1].hp = static_cast<std::int16_t>(s.units[1].maxHp - 8);
    s.units[1].marked = 1;
    const int hp = s.units[1].hp;
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 1}));
    CHECK(s.units[1].hp == hp + 5);
    CHECK(s.units[1].marked == 0);
    // A fallen ally stands up at 30%.
    MatchState f = ::Fight(def);
    TurnOf(def, f, 0);
    Give(f, 0, rice);
    Put(f, 1, Beside(f, f.units[0].cell));
    Fall(f, 1);
    REQUIRE(Apply(def, f, {ActionKind::BoardAbility, maxAbilities - 1, 1}));
    CHECK(f.units[1].hp == std::max(1, f.units[1].maxHp * 30 / 100));
}

TEST_CASE("Steal: an item from a neighbour, dropped on the thief's square when the thief is hit (campaign plan 4)", "[match][fight][effects]")
{
    MatchDef def = Data();
    const int steal = AddEffect(def, "steal", Use::Board, AbilityClass::Effect, Target::Enemy, AbilityEffect::Steal, 0, 0, 1);
    const int blow = AddAbility(def, "blow", AbilityClass::Hit, Target::Enemy, 1, 99);
    MatchState s = ::Fight(def);
    s.units[2].flags = 0;
    s.units[0].items[0] = 0;
    TurnOf(def, s, 2);
    Give(s, 2, steal);
    Put(s, 2, Beside(s, s.units[0].cell));
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 0}));
    CHECK(s.units[0].items[0] == -1);
    REQUIRE(s.stash.size() == 1);
    CHECK(s.stash[0].thief == 2);
    TurnOf(def, s, 0);
    Give(s, 0, blow);
    s.units[2].hp = 50, s.units[2].maxHp = 50;
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 2}));
    CHECK(s.stash.empty());
    CHECK(std::ranges::any_of(s.floor, [&](const Floor &f) { return f.cell == s.units[2].cell && f.item == 0; }));
}

TEST_CASE("Dart: after a battle the unit steps up to its amount away from its foe (campaign plan 4)", "[match][fight][effects]")
{
    MatchDef def = Data();
    const int dart = AddEffect(def, "runaway", Use::Passive, AbilityClass::Passive, Target::Self, AbilityEffect::Dart, 3);
    MatchState s = ::Fight(def);
    s.units[2].flags = 0;
    s.units[2].hp = s.units[2].maxHp = 99;
    s.units[2].abilities[maxAbilities - 1] = I8(dart);
    Fight(def, s, 0, 2);
    REQUIRE(Up(s.units[2]));
    const int d = Steps(s.board, s.units[0].cell, s.units[2].cell);
    CHECK(d > 1);
    CHECK(d <= 4);
}

TEST_CASE("Hasten versions: the ally also moves 2 more, or heals 3 (campaign plan 4)", "[match][fight][hasten]")
{
    MatchDef def = Data();
    const int home = AddEffect(def, "home", Use::Board, AbilityClass::Hasten, Target::Ally, AbilityEffect::HasteMove, 2, 0, 6);
    const int gift = AddEffect(def, "gift", Use::Board, AbilityClass::Hasten, Target::Ally, AbilityEffect::HasteHeal, 3, 0, 6);
    MatchState s = ::Fight(def);
    TurnOf(def, s, 0);
    Give(s, 0, home);
    Put(s, 1, Beside(s, s.units[0].cell));
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 1}));
    CHECK(s.units[1].bonusMove == 2);
    MatchState h = ::Fight(def);
    TurnOf(def, h, 0);
    Give(h, 0, gift);
    Put(h, 1, Beside(h, h.units[0].cell));
    h.units[1].hp = static_cast<std::int16_t>(h.units[1].maxHp - 5);
    const int hp = h.units[1].hp;
    REQUIRE(Apply(def, h, {ActionKind::BoardAbility, maxAbilities - 1, 1}));
    CHECK(h.units[1].hp == hp + 3);
}

TEST_CASE("Footsteps: an ally ending its turn beside the unit moves 1 more next turn (campaign plan 4)", "[match][fight][hasten]")
{
    MatchDef def = Data();
    const int steps = AddEffect(def, "steps", Use::Passive, AbilityClass::Passive, Target::Self, AbilityEffect::Footsteps, 1);
    MatchState s = ::Fight(def);
    s.units[0].abilities[maxAbilities - 1] = I8(steps);
    TurnOf(def, s, 1);
    Put(s, 1, Beside(s, s.units[0].cell));
    REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    CHECK(s.units[1].bonusMove == 1);
}

namespace
{
    // Ends turns until the round is `round` (gimmicks act at each round's start).
    void ToRound(const MatchDef &def, MatchState &s, int round)
    {
        for (int i = 0; i < 200 && s.round < round && !s.over; ++i)
            REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
        REQUIRE(s.round == round);
    }
}

TEST_CASE("Hazards: squares marked one round fall the next, hurting who stands there (campaign plan 4)", "[match][fight][gimmicks]")
{
    MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.units[2].flags = 0;
    s.gimmicks.hazardDamage = 4;
    s.gimmicks.marks = {{Mark::Hazard, s.units[0].cell}};
    const int hp = s.units[0].hp;
    ToRound(def, s, 2);
    CHECK(s.units[0].hp == hp - 4);
    CHECK(std::ranges::any_of(s.log, [](const LogEntry &e) { return e.kind == LogKind::Hazard && e.a == 0; }));
}

TEST_CASE("Hazards are marked from the fight's rng each round, on open squares (campaign plan 4)", "[match][fight][gimmicks]")
{
    MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.units[2].flags = 0;
    s.gimmicks.hazards = 2;
    s.gimmicks.hazardDamage = 1;
    ToRound(def, s, 2);
    const auto hazards = std::ranges::count_if(s.gimmicks.marks, [](const Mark &m) { return m.kind == Mark::Hazard; });
    CHECK(hazards == 2);
    for (const Mark &m : s.gimmicks.marks)
        CHECK(s.board.cells[U(m.cell)].kind != CellKind::Wall);
}

TEST_CASE("Sea wind: a unit with it takes no hazard damage (campaign plan 4)", "[match][fight][gimmicks]")
{
    MatchDef def = Data();
    const int wind = AddEffect(def, "wind", Use::Passive, AbilityClass::AfterHasten, Target::Self, AbilityEffect::SeaWind, 0, 4);
    MatchState s = ::Fight(def);
    s.units[2].flags = 0;
    s.units[0].abilities[maxAbilities - 1] = I8(wind);
    s.gimmicks.hazardDamage = 4;
    s.gimmicks.marks = {{Mark::Hazard, s.units[0].cell}};
    const int hp = s.units[0].hp;
    ToRound(def, s, 2);
    CHECK(s.units[0].hp == hp);
}

TEST_CASE("Breath: every other round a unit marks its row, and the next round strikes the other side there (campaign plan 4)", "[match][fight][gimmicks]")
{
    MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.units[2].flags = 0;
    s.gimmicks.breathUnit = 2;
    s.gimmicks.breathDamage = 5;
    s.gimmicks.breathEvery = 2;
    ToRound(def, s, 2);
    // Round 1's start marked the row (BeginMatch ran before the gimmicks were set, so round 2 marks for 3).
    const int row = s.units[2].cell / s.board.width;
    ToRound(def, s, 3);
    REQUIRE(std::ranges::any_of(s.gimmicks.marks, [](const Mark &m) { return m.kind == Mark::Breath; }));
    for (const Mark &m : s.gimmicks.marks)
        if (m.kind == Mark::Breath)
            CHECK(m.cell / s.board.width == row);
    Put(s, 0, s.gimmicks.marks.front().cell);
    const int hp = s.units[0].hp;
    ToRound(def, s, 4);
    CHECK(s.units[0].hp == hp - 5);
}

TEST_CASE("Windows: marked a round ahead, an open window lets out a spawn with its drops, up to the most alive (campaign plan 4)", "[match][fight][gimmicks]")
{
    MatchDef def = Data();
    MatchState s = ::Fight(def);
    s.units[2].flags = 0;
    const int window = Beside(s, s.units[3].cell);
    s.gimmicks.windows = {static_cast<std::int16_t>(window)};
    s.gimmicks.pattern = {{0}};
    s.gimmicks.spawn = s.units[3];
    s.gimmicks.spawn.flags = 0;
    s.gimmicks.spawnMax = 1;
    s.gimmicks.spawnDrops = {{-1, 0, 100}};
    s.gimmicks.marks = {{Mark::Window, static_cast<std::int16_t>(window)}};
    const std::size_t units = s.units.size();
    ToRound(def, s, 2);
    REQUIRE(s.units.size() == units + 1);
    CHECK(s.units.back().cell == window);
    CHECK((s.units.back().flags & flag::Spawned) != 0);
    CHECK(s.units.back().from == 3);
    CHECK(std::ranges::any_of(s.drops, [&](const Drop &d) { return d.unit == static_cast<int>(units) && d.percent == 100; }));
    ToRound(def, s, 3); // one alive already: no second
    CHECK(s.units.size() == units + 1);
}

TEST_CASE("Stolen items wait on the window nearest the thief (campaign plan 4)", "[match][fight][gimmicks]")
{
    MatchDef def = Data();
    const int steal = AddEffect(def, "steal", Use::Board, AbilityClass::Effect, Target::Enemy, AbilityEffect::Steal, 0, 0, 1);
    MatchState s = ::Fight(def);
    s.units[2].flags = 0;
    s.units[0].items[0] = 0;
    TurnOf(def, s, 2);
    Give(s, 2, steal);
    Put(s, 2, Beside(s, s.units[0].cell));
    const int near = Beside(s, s.units[2].cell);
    s.gimmicks.windows = {static_cast<std::int16_t>(near)};
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, maxAbilities - 1, 0}));
    REQUIRE(s.stash.size() == 1);
    CHECK(s.stash[0].cell == near);
}

TEST_CASE("A boss still waiting to arrive keeps the fight going (campaign plan 4)", "[match][fight][end]")
{
    const MatchDef def = Data();
    MatchState s = ::Fight(def);
    // Unit 3 becomes a second boss that arrives at round 9.
    s.units[3].flags = flag::Boss;
    const std::int16_t square = static_cast<std::int16_t>(s.units[3].cell);
    s.units[3].cell = -1;
    s.arrivals.push_back({3, square, 9});
    s.units[2].hp = 1;
    Fight(def, s, 0, 2);
    CHECK_FALSE(s.over);
}

TEST_CASE("A battle heal mends the ally it names, else the most hurt one of its group (feedback 2026-11-24 16:57, note 19)", "[match][fight][heal]")
{
    MatchDef def = Data();
    const int mend = AddEffect(def, "mend_ally", Use::Battle, AbilityClass::Heal, Target::Ally, AbilityEffect::None, 0, 5, 1);
    for (const int named : {0, -1})
    {
        MatchState s = ::Fight(def);
        s.units[3].flags = 0, s.units[3].cell = -1; // only the boss stands against them
        TurnOf(def, s, 1);
        Give(s, 1, mend);
        Put(s, 1, Beside(s, s.units[2].cell));
        Put(s, 0, s.units[1].cell); // the two on one square fight together
        s.now.origin = s.units[1].cell;
        s.units[0].hp = static_cast<std::int16_t>(s.units[0].maxHp - 8);
        s.units[1].hp = static_cast<std::int16_t>(s.units[1].maxHp - 1);
        REQUIRE(Apply(def, s, {ActionKind::Attack, I8(2)}));
        REQUIRE(s.battle.on);
        REQUIRE(std::ranges::find(s.battle.fighters, I8(0)) != s.battle.fighters.end());
        // 1. The first time the healer picks, it mends the player (named, or as the most hurt), not itself.
        bool healed = false;
        for (int i = 0; i < 20 && s.battle.on && !healed; ++i)
            if (Current(s) == 1)
            {
                const int hp0 = s.units[0].hp, hp1 = s.units[1].hp;
                REQUIRE(Check(def, s, {ActionKind::BattleAbility, maxAbilities - 1, I8(named)}) == Why::Ok);
                REQUIRE(Apply(def, s, {ActionKind::BattleAbility, maxAbilities - 1, I8(named)}));
                INFO("named " << named);
                CHECK(s.units[0].hp == std::min<int>(s.units[0].maxHp, hp0 + 5));
                CHECK(s.units[1].hp == hp1);
                healed = true;
            }
            else
                REQUIRE(Apply(def, s, {ActionKind::BattleAbility, 0}));
        REQUIRE(healed);
    }
}
