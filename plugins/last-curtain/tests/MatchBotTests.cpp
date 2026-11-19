#include "MatchTestData.hpp"

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
}

TEST_CASE("Bots play matches to the end on every seed (T303)", "[match][bots]")
{
    const MatchDef def = Data();
    for (std::uint64_t seed = 1; seed <= 50; ++seed)
    {
        MatchState s = Start(def, seed);
        int applies = 0;
        while (Current(s) >= 0 && applies < 20000)
        {
            const Action a = ChooseAction(def, s, BotLevel::Medium);
            REQUIRE(Apply(def, s, a));
            ++applies;
        }
        CHECK(s.over);
        CHECK(applies < 20000);
    }
}

TEST_CASE("A carrier with essence heads for the stage and Performs (T304)", "[match][bots]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int me = Current(s);
    // Three steps from the stage, on a free square.
    int from = -1;
    for (int c = 0; c < static_cast<int>(s.board.cells.size()) && from < 0; ++c)
        if (Steps(s.board, c, s.stage) == 3 && std::ranges::none_of(s.units, [&](const Unit &u) { return u.cell == c; }))
            from = c;
    REQUIRE(from >= 0);
    s.units[U(me)].cell = static_cast<std::int16_t>(from);
    s.now.origin = static_cast<std::int16_t>(from);
    s.units[U(me)].essence = 10;
    int myTurns = 0;
    const auto performed = [&] { return std::ranges::any_of(s.log, [&](const LogEntry &e) { return e.kind == LogKind::Performed && e.a == me; }); };
    while (!performed() && myTurns < 2 && Current(s) >= 0)
    {
        if (Current(s) != me)
        {
            // Everyone else just passes (or answers a battle or a level choice).
            const bool busy = s.battle.on || s.units[U(Current(s))].pending >= 0;
            REQUIRE(Apply(def, s, busy ? ChooseAction(def, s, BotLevel::Hard) : Action{ActionKind::EndTurn}));
            continue;
        }
        const Action a = ChooseAction(def, s, BotLevel::Hard);
        if (a.kind == ActionKind::EndTurn)
            ++myTurns;
        REQUIRE(Apply(def, s, a));
    }
    CHECK(performed());
}

TEST_CASE("A bot that cannot afford anything still acts (T305)", "[match][bots]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    for (Unit &u : s.units)
        if (u.kind == 0)
            u.hp = 1;
    for (int i = 0; i < 400 && Current(s) >= 0; ++i)
    {
        const Action a = ChooseAction(def, s, BotLevel::Easy);
        CHECK(Check(def, s, a) == Why::Ok);
        REQUIRE(Apply(def, s, a));
    }
}

#include <MatchSoak.hpp>

TEST_CASE("The soak counts sources and comebacks (T306)", "[match][bots]")
{
    const MatchDef def = Data();
    const MatchReport r = SoakMatches(def, 1, 20, BotLevel::Medium, BotLevel::Medium);
    CHECK(r.runs == 20);
    CHECK(r.faults == 0);
    CHECK(r.wins[0] + r.wins[1] + r.draws == 20);
    CHECK(r.fromMonsters > 0);
    CHECK(r.decidedRound >= 1);
    CHECK(r.decidedRound <= def.options.rounds);
    CHECK_FALSE(Text(def, r).empty());
}

TEST_CASE("Late centre monsters are worth more essence than early ones (T321)", "[match][bots]")
{
    const MatchDef def = Data();
    double early = 0, late = 0;
    int earlyCount = 0, lateCount = 0;
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        MatchState s = Start(def, seed);
        std::size_t seen = s.log.size();
        for (int applies = 0; Current(s) >= 0 && applies < 20000; ++applies)
        {
            REQUIRE(Apply(def, s, ChooseAction(def, s, BotLevel::Medium)));
            // 1. Every monster from a centre square downed since the last action, by the round it fell.
            for (; seen < s.log.size(); ++seen)
            {
                const LogEntry &e = s.log[seen];
                if (e.kind != LogKind::Down || s.units[U(e.a)].kind != 1 ||
                    std::ranges::find(s.board.centreSpawns, s.units[U(e.a)].home) == s.board.centreSpawns.end())
                    continue;
                if (s.round < 10)
                    early += e.c, ++earlyCount;
                else if (s.round > 20)
                    late += e.c, ++lateCount;
            }
        }
    }
    REQUIRE(earlyCount > 0);
    REQUIRE(lateCount > 0);
    CHECK(late / lateCount > early / earlyCount);
}

TEST_CASE("The faction match is in balance (T307)", "[match-balance]")
{
    const MatchDef def = Data();
    const MatchReport r = SoakMatches(def, 1, 400, BotLevel::Medium, BotLevel::Medium);
    INFO(Text(def, r));
    CHECK(r.Holds(def));
}

TEST_CASE("A bot does not walk a hurt character into a hunter's sight (T320)", "[match][bots]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int me = Current(s);
    Unit &u = s.units[U(me)];
    // 1. The hunter on the centre spawn square nearer my hub; I stand two steps off it, beyond it from home.
    int hunter = -1;
    for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
        if (s.units[U(i)].kind == 1 && s.units[U(i)].cell >= 0 && def.monsters[U(s.units[U(i)].def)].behaviour == Behaviour::Hunt)
            hunter = i;
    REQUIRE(hunter >= 0);
    const int h = s.units[U(hunter)].cell, sight = def.monsters[U(s.units[U(hunter)].def)].sight;
    int spot = -1;
    for (int c = 0; c < static_cast<int>(s.board.cells.size()) && spot < 0; ++c)
        if (s.board.cells[U(c)].kind == CellKind::Centre && c != s.stage && Steps(s.board, h, c) == 2 &&
            Steps(s.board, c, u.home) > Steps(s.board, h, u.home) &&
            std::ranges::none_of(s.units, [&](const Unit &o) { return o.cell == c && o.hp > 0; }))
            spot = c;
    REQUIRE(spot >= 0);
    u.cell = s.now.origin = static_cast<std::int16_t>(spot);
    u.hp = static_cast<std::int16_t>(u.maxHp / 4);
    u.move = 3;
    s.now.movesLeft = 3;
    // 2. Its whole move ends out of the hunter's sight.
    for (int i = 0; i < 8; ++i)
    {
        const Action a = ChooseAction(def, s, BotLevel::Hard);
        if (a.kind != ActionKind::Move)
            break;
        REQUIRE(Apply(def, s, a));
    }
    CHECK(Steps(s.board, s.units[U(me)].cell, h) > sight);
}
