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
