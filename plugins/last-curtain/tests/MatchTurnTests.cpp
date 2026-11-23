#include "MatchTestData.hpp"

#include <Match.hpp>
#include <MatchBots.hpp>

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
        // An open step that does not lead back to where the turn began.
        const int here = s.units[static_cast<std::size_t>(Current(s))].cell;
        // Prefer a step after which another such step is open (a hub ringed by friends is a dead end).
        for (int pass = 0; pass < 2; ++pass)
            for (int d = 0; d < 4; ++d)
                if (Check(def, s, {ActionKind::Move, static_cast<std::int8_t>(d)}) == Why::Ok && Neighbour(s.board, here, d) != s.now.origin)
                {
                    MatchState t = s;
                    Apply(def, t, {ActionKind::Move, static_cast<std::int8_t>(d)});
                    const int there = t.units[static_cast<std::size_t>(Current(t))].cell;
                    for (int e = 0; pass == 0 && e < 4; ++e)
                        if (Check(def, t, {ActionKind::Move, static_cast<std::int8_t>(e)}) == Why::Ok && Neighbour(t.board, there, e) != t.now.origin)
                            return d;
                    if (pass == 1)
                        return d;
                }
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
    def.modes.at("faction").roundLimit = 2; // the limit lives in the mode (campaign plan 2)
    auto playOut = [&](MatchState &s) {
        int applies = 0;
        while (Current(s) >= 0 && applies++ < 100)
            REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    };
    MatchState s = Start(def);
    CHECK(TurnsLeft(def, s) == 2);
    while (s.round == 1)
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

TEST_CASE("Two of one side may share a square, never with a foe, never three (T355)", "[match][turn]")
{
    const MatchDef def = Data();
    REQUIRE(def.rules.board.squareCapacity == 2);
    MatchState s = Start(def);
    const int me = Current(s);
    Unit &u = s.units[U(me)];
    // 1. A square beside me with a friend on it: I may step in.
    int dir = -1, friendUnit = -1, foeUnit = -1;
    for (int d = 0; d < 4 && dir < 0; ++d)
        if (const int n = Neighbour(s.board, u.cell, d); n >= 0 && n != s.stage && n != s.board.hub[1 - u.faction] &&
                                                         std::ranges::none_of(s.units, [&](const Unit &o) { return o.cell == n; }))
            dir = d;
    REQUIRE(dir >= 0);
    const int n = Neighbour(s.board, u.cell, dir);
    for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
        if (i != me && s.units[U(i)].kind == 0 && s.units[U(i)].cell >= 0)
            (s.units[U(i)].faction == u.faction ? friendUnit : foeUnit) = i;
    REQUIRE(friendUnit >= 0);
    REQUIRE(foeUnit >= 0);
    s.units[U(friendUnit)].cell = static_cast<std::int16_t>(n);
    CHECK(Check(def, s, {ActionKind::Move, static_cast<std::int8_t>(dir)}) == Why::Ok);
    // 2. Not with a foe there.
    s.units[U(friendUnit)].cell = static_cast<std::int16_t>(-1);
    s.units[U(foeUnit)].cell = static_cast<std::int16_t>(n);
    CHECK(Check(def, s, {ActionKind::Move, static_cast<std::int8_t>(dir)}) == Why::Blocked);
    // 3. Not as a third.
    s.units[U(foeUnit)].cell = static_cast<std::int16_t>(-1);
    int second = -1;
    for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
        if (i != me && i != friendUnit && s.units[U(i)].faction == u.faction && s.units[U(i)].cell >= 0)
            second = i;
    REQUIRE(second >= 0);
    s.units[U(friendUnit)].cell = s.units[U(second)].cell = static_cast<std::int16_t>(n);
    CHECK(Check(def, s, {ActionKind::Move, static_cast<std::int8_t>(dir)}) == Why::Blocked);
}

TEST_CASE("Every unit, monsters included, takes its turn by Speed (campaign plan 2)", "[match][turn][order]")
{
    const MatchDef def = Data();
    const MatchState s = Start(def);
    CHECK(s.order.size() == s.units.size());
    for (std::size_t i = 1; i < s.order.size(); ++i)
        CHECK(s.units[U(s.order[i - 1])].speed >= s.units[U(s.order[i])].speed);
    CHECK(Start(def).order == s.order); // ties are the same for the same seed
}

TEST_CASE("A monster acts at its own place in the order (campaign plan 2)", "[match][turn][order]")
{
    MatchDef def = Data();
    const int prompter = MatchDef::Find(def.monsters, "prompter"); // walks toward the stage
    MatchState s = Start(def);
    // 1. A fast monster and a slow one, two steps from the stage; both join the order next round.
    const auto add = [&](int speed) {
        Unit u = FromMonster(def, prompter, -1, -1);
        u.speed = static_cast<std::int16_t>(speed);
        for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
            if (Steps(s.board, c, s.stage) == 2 && std::ranges::none_of(s.units, [&](const Unit &o) { return o.cell == c; }))
                u.cell = static_cast<std::int16_t>(c);
        s.units.push_back(u);
        return static_cast<int>(s.units.size()) - 1;
    };
    const int fast = add(50), slow = add(0);
    // 2. Round 1 ends; in round 2 the fast one has walked before the first character's turn, the slow one not yet.
    while (s.round == 1)
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    REQUIRE(s.units[U(Current(s))].kind == 0);
    CHECK(Steps(s.board, s.units[U(fast)].cell, s.stage) == 1);
    CHECK(Steps(s.board, s.units[U(slow)].cell, s.stage) == 2);
    CHECK(s.order.front() == fast);
}

TEST_CASE("A unit that appears mid-fight acts from the next round (campaign plan 2)", "[match][turn][order]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    // A monster that appeared in round 2 (from = 3) waits through round 2, then acts.
    Unit u = FromMonster(def, MatchDef::Find(def.monsters, "prompter"), -1, -1);
    u.speed = 50;
    for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
        if (Steps(s.board, c, s.stage) == 3 && std::ranges::none_of(s.units, [&](const Unit &o) { return o.cell == c; }))
            u.cell = static_cast<std::int16_t>(c);
    u.from = 3;
    s.units.push_back(u);
    const int m = static_cast<int>(s.units.size()) - 1;
    while (s.round == 1)
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    CHECK(Steps(s.board, s.units[U(m)].cell, s.stage) == 3);
    while (s.round == 2)
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    CHECK(Steps(s.board, s.units[U(m)].cell, s.stage) < 3);
}

TEST_CASE("A spawned monster joins from the round after it appears (campaign plan 2)", "[match][turn][order]")
{
    const MatchDef def = Data();
    MatchState s = Start(def, 11);
    for (int i = 0; i < 20000 && Current(s) >= 0; ++i)
    {
        const std::size_t logged = s.log.size();
        const int round = s.round;
        REQUIRE(Apply(def, s, ChooseAction(def, s, BotLevel::Hard)));
        for (std::size_t l = logged; l < s.log.size(); ++l)
            if (s.log[l].kind == LogKind::Spawned && round >= 2)
            {
                CHECK(s.units[U(s.log[l].b)].from > round);
                return;
            }
    }
    FAIL("no spawn after round 1");
}

TEST_CASE("A round where no one else can act still moves on (campaign plan 2)", "[match][turn][order]")
{
    const MatchDef def = Data();
    MatchState s = Fight(def);
    // Everyone but the player has fallen; the ally is revived but waits for the next round.
    for (int u : {1, 2, 3})
        s.units[U(u)].hp = 0;
    s.units[1].hp = 5;
    s.units[1].from = static_cast<std::int16_t>(s.round + 1);
    while (Current(s) != 0)
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    const int round = s.round;
    REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    CHECK(s.round == round + 1);
    std::vector<int> acted{Current(s)};
    REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    acted.push_back(Current(s));
    CHECK(std::ranges::count(acted, 1) == 1); // the revived ally has its turn in the new round
}
