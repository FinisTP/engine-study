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

    int Ability(const MatchDef &def, const char *id) { return MatchDef::Find(def.abilities, id); }

    // A hall square with a free hall square north or south of it, away from everyone.
    std::pair<int, int> FreePair(const MatchState &s)
    {
        const Board &b = s.board;
        for (int c = b.width * 3; c < b.width * (b.height - 3); ++c)
            for (int d : {0, 2})
                if (const int n = Neighbour(b, c, d); n >= 0 && b.cells[U(c)].kind == CellKind::Hall && b.cells[U(n)].kind == CellKind::Hall &&
                    std::ranges::none_of(s.units, [&](const Unit &u) { return u.cell == c || u.cell == n; }))
                    return {c, n};
        return {-1, -1};
    }

    // The current character and an enemy character side by side; returns {attacker, defender}.
    std::pair<int, int> Face(MatchState &s)
    {
        const int a = Current(s);
        int d = -1;
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[U(i)].kind == 0 && s.units[U(i)].faction != s.units[U(a)].faction)
                d = i;
        const auto [c, n] = FreePair(s);
        REQUIRE(c >= 0);
        s.units[U(a)].cell = static_cast<std::int16_t>(c);
        s.units[U(d)].cell = static_cast<std::int16_t>(n);
        s.now.origin = static_cast<std::int16_t>(c);
        return {a, d};
    }

    // A monster of the given tier next to the current character.
    int MonsterNext(MatchState &s, const MatchDef &def, Tier tier)
    {
        const int a = Current(s);
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[U(i)].kind == 1 && def.monsters[U(s.units[U(i)].def)].tier == tier)
            {
                const auto [c, n] = FreePair(s);
                s.units[U(a)].cell = static_cast<std::int16_t>(c);
                s.units[U(i)].cell = static_cast<std::int16_t>(n);
                s.now.origin = static_cast<std::int16_t>(c);
                return i;
            }
        return -1;
    }

    int Blows(const MatchState &s) { return static_cast<int>(std::ranges::count(s.log, LogKind::Blow, &LogEntry::kind)); }
}

TEST_CASE("Damage is power plus Attack minus Defence, at least 1 (T287)", "[match][battle]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const auto [a, d] = Face(s);
    s.units[U(a)].attack = 5;
    s.units[U(d)].defence = 3;
    const int bite = Ability(def, "bite"); // power 4
    CHECK(Damage(def, s, bite, a, d, 0) == 6);
    s.units[U(d)].defence = 20;
    CHECK(Damage(def, s, bite, a, d, 0) == 1);
    s.units[U(d)].defence = 3;
    REQUIRE(Apply(def, s, {ActionKind::Attack, I8(d)}));
    s.battle.shield[1] = 2;
    CHECK(Damage(def, s, bite, a, d, 0) == 4);
    s.battle.shield[1] = 0;
    s.units[U(a)].abilities[1] = I8(Ability(def, "gamble")); // 2d6, costs 2
    const Forecast f = Preview(def, s, {ActionKind::BattleAbility, 1});
    CHECK(f.dealtMin == 2 + 5 - 3);
    CHECK(f.dealtMax == 12 + 5 - 3);
    CHECK(f.cost == 2);
    CHECK(Preview(def, s, {ActionKind::BattleAbility, 0}).cost == 0);
}

TEST_CASE("A battle runs three rounds, attacker first, one pick each turn (T288)", "[match][battle]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const auto [a, d] = Face(s);
    s.units[U(a)].hp = s.units[U(a)].maxHp = 99;
    s.units[U(d)].hp = s.units[U(d)].maxHp = 99;
    CHECK(Check(def, s, {ActionKind::Attack, I8(a)}) == Why::OutOfRange); // not itself
    REQUIRE(Apply(def, s, {ActionKind::Attack, I8(d)}));
    CHECK(Check(def, s, {ActionKind::Move, 0}) == Why::MustChoose);
    for (int i = 0; i < 6; ++i)
    {
        CHECK(Current(s) == (i % 2 == 0 ? a : d));
        REQUIRE(Apply(def, s, {ActionKind::BattleAbility, 0}));
    }
    CHECK(Blows(s) == 6);
    CHECK_FALSE(s.battle.on);
    CHECK(s.now.acted);
    CHECK(Current(s) == a);
    CHECK(Check(def, s, {ActionKind::Attack, I8(d)}) == Why::AlreadyActed);

    // A defender with the Duellist passive picks first.
    MatchState t = Start(def);
    const auto [a2, d2] = Face(t);
    t.units[U(d2)].abilities[7] = I8(Ability(def, "duel"));
    REQUIRE(Apply(def, t, {ActionKind::Attack, I8(d2)}));
    CHECK(Current(t) == d2);
}

TEST_CASE("A carrier downed by a rival gives up its essence; by a monster, drops it (T289)", "[match][battle]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const auto [a, d] = Face(s);
    s.units[U(d)].essence = 9;
    s.units[U(d)].hp = 1;
    REQUIRE(Apply(def, s, {ActionKind::Attack, I8(d)}));
    REQUIRE(Apply(def, s, {ActionKind::BattleAbility, 0}));
    CHECK(s.units[U(a)].essence == 9);
    CHECK(s.units[U(d)].essence == 0);
    CHECK(s.units[U(d)].cell == -1);
    CHECK(s.units[U(a)].kills == 1);
    CHECK_FALSE(s.battle.on);

    MatchState t = Start(def);
    const int me = Current(t);
    const int m = MonsterNext(t, def, Tier::Strong);
    REQUIRE(m >= 0);
    const int where = t.units[U(me)].cell;
    t.units[U(me)].essence = 9;
    t.units[U(me)].hp = 1;
    t.units[U(m)].attack = 50;
    t.units[U(m)].hp = 99;
    REQUIRE(Apply(def, t, {ActionKind::Attack, I8(m)}));
    REQUIRE(Apply(def, t, {ActionKind::BattleAbility, 0})); // the monster answers at once
    CHECK(t.units[U(me)].cell == -1);
    CHECK(std::ranges::any_of(t.floor, [&](const Floor &f) { return f.cell == where && f.essence == 9; }));
}

TEST_CASE("A downed fighter misses one turn and returns home whole (T290)", "[match][battle]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int me = Current(s);
    const int m = MonsterNext(s, def, Tier::Strong);
    s.units[U(me)].hp = 1;
    s.units[U(m)].attack = 50;
    s.units[U(m)].hp = 99;
    REQUIRE(Apply(def, s, {ActionKind::Attack, I8(m)}));
    REQUIRE(Apply(def, s, {ActionKind::BattleAbility, 0}));
    CHECK(Current(s) == s.order[1]); // its turn passed with it
    // Everyone else ends their turns until its turn comes round: it is skipped and comes back.
    const auto back = [&] { return std::ranges::any_of(s.log, [&](const LogEntry &e) { return e.kind == LogKind::Back && e.a == me; }); };
    while (!back())
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    CHECK(s.round == 2);
    CHECK(Current(s) != me); // the turn it came back on is spent
    CHECK(s.units[U(me)].cell == s.units[U(me)].home);
    CHECK(s.units[U(me)].hp == s.units[U(me)].maxHp);
    while (Current(s) != me)
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    CHECK(s.round == 3);
}

TEST_CASE("Stepping onto your own hub restores full HP (T291)", "[match][battle]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int me = Current(s);
    Unit &u = s.units[U(me)];
    u.hp = 3;
    int dir = -1;
    for (int d = 0; d < 4; ++d)
        if (Neighbour(s.board, u.cell, d) == u.home)
            dir = d;
    REQUIRE(dir >= 0);
    REQUIRE(Apply(def, s, {ActionKind::Move, I8(dir)}));
    REQUIRE(Apply(def, s, {ActionKind::Undo}));
    CHECK(u.hp == 3); // walking in and back out heals nothing
    REQUIRE(Apply(def, s, {ActionKind::Move, I8(dir)}));
    REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    CHECK(s.units[U(me)].hp == s.units[U(me)].maxHp);
}
