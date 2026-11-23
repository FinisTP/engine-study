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
        for (int i = 0; i < 50 && s.battle.on; ++i)
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
