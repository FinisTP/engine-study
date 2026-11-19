#include "MatchTestData.hpp"

#include <Match.hpp>

#include <algorithm>

using namespace umbra::match;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }
    std::int8_t I8(int i) { return static_cast<std::int8_t>(i); }

    MatchState Start(const MatchDef &def)
    {
        return NewMatch(def, 7, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
    }

    // The direction from a square to its neighbour, or -1.
    int Toward(const Board &b, int from, int to)
    {
        for (int d = 0; d < 4; ++d)
            if (Neighbour(b, from, d) == to)
                return d;
        return -1;
    }

    // Puts the current character one step from the stage; returns the direction onto it.
    int NextToStage(MatchState &s)
    {
        const int me = Current(s);
        for (int d = 0; d < 4; ++d)
            if (const int n = Neighbour(s.board, s.stage, d); n >= 0)
            {
                s.units[U(me)].cell = static_cast<std::int16_t>(n);
                s.now.origin = static_cast<std::int16_t>(n);
                return Toward(s.board, n, s.stage);
            }
        return -1;
    }
}

TEST_CASE("Only Perform turns essence into score (T296)", "[match][stage]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int me = Current(s);
    const int side = s.units[U(me)].faction;
    s.units[U(me)].essence = 12;
    CHECK(Check(def, s, {ActionKind::Perform}) == Why::NotOnStage);
    const int dir = NextToStage(s);
    REQUIRE(Apply(def, s, {ActionKind::Move, I8(dir)}));
    CHECK(s.score[U(side)] == 0);
    REQUIRE(Apply(def, s, {ActionKind::Perform}));
    CHECK(s.score[U(side)] == 12);
    CHECK(s.units[U(me)].essence == 0);
    CHECK(s.now.acted);
    CHECK(std::ranges::any_of(s.log, [&](const LogEntry &e) { return e.kind == LogKind::Performed && e.a == me && e.b == 12; }));

    MatchState t = Start(def);
    NextToStage(t);
    t.units[U(Current(t))].cell = t.stage;
    CHECK(Check(def, t, {ActionKind::Perform}) == Why::NothingToPerform);
}

TEST_CASE("The stage holds one (T297)", "[match][stage]")
{
    MatchDef def = Data();
    def.rules.board.squareCapacity = 2; // even where squares hold two, the stage holds one
    MatchState s = Start(def);
    const int me = Current(s);
    const int dir = NextToStage(s);
    const int other = me == 3 ? 4 : 3;
    s.units[U(other)].cell = s.stage;
    CHECK(Check(def, s, {ActionKind::Move, I8(dir)}) == Why::Blocked);
}
