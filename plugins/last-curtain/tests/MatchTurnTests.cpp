#include "MatchTestData.hpp"

#include <Match.hpp>

#include <algorithm>

using namespace umbra::match;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }

    MatchState Start(const MatchDef &def, std::uint64_t seed = 7)
    {
        return NewMatch(def, seed, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
    }

    bool Has(const std::vector<Action> &legal, ActionKind kind)
    {
        return std::ranges::any_of(legal, [&](const Action &a) { return a.kind == kind; });
    }

    // The first direction the current unit can step in, or -1.
    int OpenStep(const MatchDef &def, const MatchState &s)
    {
        for (int d = 0; d < 4; ++d)
            if (Check(def, s, {ActionKind::Move, static_cast<std::int8_t>(d)}) == Why::Ok)
                return d;
        return -1;
    }
}

TEST_CASE("A move can be undone until the turn's action (T283)", "[match][turn]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int me = Current(s);
    REQUIRE(me == s.order[0]);
    const int origin = s.units[U(me)].cell;
    for (int i = 0; i < 2; ++i)
    {
        const int d = OpenStep(def, s);
        REQUIRE(d >= 0);
        REQUIRE(Apply(def, s, {ActionKind::Move, static_cast<std::int8_t>(d)}));
    }
    CHECK(s.now.movesLeft == s.units[U(me)].move - 2);
    REQUIRE(Apply(def, s, {ActionKind::Undo}));
    CHECK(s.units[U(me)].cell == origin);
    CHECK(s.now.movesLeft == s.units[U(me)].move);
    // An item under the unit: picking it up is the action; then no more moving.
    s.floor.push_back({static_cast<std::int16_t>(origin), 0, 0});
    REQUIRE(Apply(def, s, {ActionKind::PickUp, static_cast<std::int8_t>(s.floor.size() - 1)}));
    CHECK(s.units[U(me)].items[0] == 0);
    const auto legal = Legal(def, s);
    CHECK_FALSE(Has(legal, ActionKind::Move));
    CHECK_FALSE(Has(legal, ActionKind::Undo));
    CHECK(Has(legal, ActionKind::EndTurn));
    CHECK(Check(def, s, {ActionKind::Move, 0}) == Why::AlreadyActed);
    CHECK_FALSE(Apply(def, s, {ActionKind::Move, 0}));
}

TEST_CASE("A character moves at most its Move a turn (T284)", "[match][turn]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int me = Current(s);
    for (int i = 0; i < s.units[U(me)].move; ++i)
    {
        const int d = OpenStep(def, s);
        REQUIRE(d >= 0);
        REQUIRE(Apply(def, s, {ActionKind::Move, static_cast<std::int8_t>(d)}));
    }
    for (int d = 0; d < 4; ++d)
        CHECK(Check(def, s, {ActionKind::Move, static_cast<std::int8_t>(d)}) != Why::Ok);
    CHECK(Check(def, s, {ActionKind::Move, 0}) == Why::NoMovesLeft);
    REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    CHECK(Current(s) == s.order[1]);
    CHECK(s.now.movesLeft == s.units[U(s.order[1])].move);
}

TEST_CASE("Auto end turn ends the turn after the action (T285)", "[match][turn]")
{
    MatchDef def = Data();
    def.options.autoEndTurn = true;
    MatchState s = Start(def);
    const int me = Current(s);
    s.floor.push_back({s.units[U(me)].cell, 0, 0});
    REQUIRE(Apply(def, s, {ActionKind::PickUp, static_cast<std::int8_t>(s.floor.size() - 1)}));
    CHECK(Current(s) == s.order[1]);
}

TEST_CASE("The match ends after its rounds, with the tie rule (T286)", "[match][turn]")
{
    MatchDef def = Data();
    def.options.rounds = 2;
    auto playOut = [&](MatchState &s) {
        int applies = 0;
        while (Current(s) >= 0 && applies++ < 100)
            REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    };
    MatchState s = Start(def);
    CHECK(TurnsLeft(def, s) == 2);
    for (std::size_t i = 0; i < s.order.size(); ++i)
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    CHECK(s.round == 2);
    CHECK(TurnsLeft(def, s) == 1);
    playOut(s);
    CHECK(s.over);
    CHECK(Current(s) == -1);
    CHECK(s.winner == -1); // equal score, both guardians, no kills: a draw
    CHECK(Check(def, s, {ActionKind::EndTurn}) == Why::Over);

    MatchState t = Start(def);
    for (Unit &u : t.units)
        if (u.kind == 1 && def.monsters[U(u.def)].tier == Tier::Guardian && u.faction == 1)
            u.hp = 0, u.cell = -1;
    playOut(t);
    CHECK(t.winner == 0); // side 1 lost its guardian

    MatchState v = Start(def);
    v.score = {5, 9};
    playOut(v);
    CHECK(v.winner == 1);
}
