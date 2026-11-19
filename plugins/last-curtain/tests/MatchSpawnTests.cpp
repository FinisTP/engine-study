#include "MatchTestData.hpp"

#include <Match.hpp>
#include <MatchSave.hpp>

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

    bool Taken(const MatchState &s, int c) { return std::ranges::any_of(s.units, [&](const Unit &u) { return u.cell == c && u.hp > 0; }); }

    int MonsterOn(const MatchState &s, int cell)
    {
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[U(i)].kind == 1 && s.units[U(i)].home == cell)
                return i;
        return -1;
    }

    // The current character downs the monster that spawned on `den` in one blow.
    void Kill(const MatchDef &def, MatchState &s, int den)
    {
        const int me = Current(s), m = MonsterOn(s, den);
        REQUIRE(m >= 0);
        int beside = -1;
        for (int d = 0; d < 4; ++d)
            if (const int n = Neighbour(s.board, den, d); n >= 0 && !Taken(s, n))
                beside = n;
        REQUIRE(beside >= 0);
        s.units[U(me)].cell = s.now.origin = static_cast<std::int16_t>(beside);
        s.units[U(me)].attack = 99;
        REQUIRE(Apply(def, s, {ActionKind::Attack, I8(m)}));
        REQUIRE(Apply(def, s, {ActionKind::BattleAbility, 0}));
        REQUIRE(s.units[U(m)].cell == -1);
        s.units[U(me)].cell = s.units[U(me)].home; // out of the way
        if (s.units[U(me)].pending >= 0)
            REQUIRE(Apply(def, s, {ActionKind::LevelUp, 0}));
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    }

    const Spawn *Pending(const MatchState &s, int cell)
    {
        const auto it = std::ranges::find(s.spawns, static_cast<std::int16_t>(cell), &Spawn::cell);
        return it == s.spawns.end() ? nullptr : &*it;
    }

    void EndRound(const MatchDef &def, MatchState &s)
    {
        const int round = s.round;
        while (s.round == round && Current(s) >= 0)
            REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    }
}

TEST_CASE("The stage stays put and monsters never stand on it (T312)", "[match][spawn]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const Board &b = s.board;
    CHECK(s.stage == (b.height / 2) * b.width + b.width / 2);
    REQUIRE_FALSE(b.centreSpawns.empty());
    for (int c : b.centreSpawns)
    {
        CHECK(c != s.stage);
        CHECK(std::ranges::find(b.centreSpawns, Mirror(b, c)) != b.centreSpawns.end());
        CHECK(MonsterOn(s, c) >= 0); // filled at the start
    }
    // 1. A monster that goes for the stage stops beside it.
    for (Unit &u : s.units)
        if (u.kind == 1 && def.monsters[U(u.def)].behaviour == Behaviour::Stage)
            u.cell = -1;
    const int prompter = MatchDef::Find(def.monsters, "prompter");
    for (int d = 0; d < 4; ++d)
        if (const int n = Neighbour(b, s.stage, d); n >= 0 && !Taken(s, n))
        {
            for (int k = 0; k < 4; ++k)
                if (const int far = Neighbour(b, n, k); far >= 0 && far != s.stage && !Taken(s, far) && Steps(b, far, s.stage) == 2)
                {
                    Unit u = s.units.back();
                    u.def = I8(prompter);
                    u.hp = u.maxHp = static_cast<std::int16_t>(def.monsters[U(prompter)].hp);
                    u.speed = 3, u.cell = u.home = static_cast<std::int16_t>(far);
                    s.units.push_back(u);
                    break;
                }
            break;
        }
    const int stage = s.stage;
    for (int r = 0; r < 6; ++r)
    {
        EndRound(def, s);
        CHECK(s.stage == stage);
        CHECK(std::ranges::none_of(s.units, [&](const Unit &u) { return u.kind == 1 && u.cell == stage; }));
    }
    CHECK(Steps(b, s.units.back().cell, stage) == 1);
}

TEST_CASE("An emptied square announces its next spawn and fills it, waiting while occupied (T313)", "[match][spawn]")
{
    MatchDef def = Data();
    MatchState s = Start(def);
    const int den = s.board.weakDens[0], mirror = Mirror(s.board, den);
    const int every = def.spawns[0].every; // weak, from 1
    REQUIRE(def.spawns[0].zone == 0);

    // 1. Killed in round 1: the next one is due `every` rounds on; the mirror is still alive, so nothing there.
    Kill(def, s, den);
    const Spawn *p = Pending(s, den);
    REQUIRE(p);
    const int due = 1 + every;
    CHECK(p->round == due);
    CHECK(p->monster == def.spawns[0].monster);
    CHECK_FALSE(Pending(s, mirror));

    // 2. The mirror dies a round later and spawns together with the first.
    EndRound(def, s);
    Kill(def, s, mirror);
    REQUIRE(Pending(s, mirror));
    CHECK(Pending(s, mirror)->round == due);

    // 3. Announced `spawn.warn` rounds before; a character on the square holds it off.
    while (s.round < due - def.rules.spawn.warn)
        EndRound(def, s);
    CHECK(std::ranges::any_of(s.log, [&](const LogEntry &e) { return e.kind == LogKind::SpawnSoon && e.a == den; }));
    const int sitter = s.order[1];
    s.units[U(sitter)].cell = static_cast<std::int16_t>(den);
    while (s.round < due)
        EndRound(def, s);
    CHECK(MonsterOn(s, mirror) >= 0);
    CHECK(s.units[U(MonsterOn(s, mirror))].cell == mirror);
    CHECK(s.units[U(MonsterOn(s, den))].cell == -1);
    REQUIRE(Pending(s, den));
    s.units[U(sitter)].cell = s.units[U(sitter)].home;
    REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    const Unit &born = s.units[U(MonsterOn(s, den))];
    CHECK(born.cell == den);
    CHECK(born.hp == born.maxHp);
    CHECK_FALSE(Pending(s, den));
    CHECK(std::ranges::any_of(s.log, [&](const LogEntry &e) { return e.kind == LogKind::Spawned && e.a == den; }));

    // 4. The kind follows the zone's row for the round it is due.
    MatchDef later = Data();
    const int stagehand = MatchDef::Find(later.monsters, "stagehand");
    later.spawns.insert(later.spawns.begin() + 1, SpawnRow{0, 2, stagehand, 4});
    MatchState t = Start(later);
    Kill(later, t, t.board.weakDens[0]);
    REQUIRE(Pending(t, t.board.weakDens[0]));
    CHECK(Pending(t, t.board.weakDens[0])->monster == stagehand);
}

TEST_CASE("Spawns survive a save (T314)", "[match][spawn][save]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    Kill(def, s, s.board.weakDens[0]);
    REQUIRE_FALSE(s.spawns.empty());
    MatchState loaded;
    std::string error;
    nlohmann::json j = SaveMatch(s);
    CHECK(j["version"] == 2);
    REQUIRE(LoadMatch(j, loaded, error));
    CHECK(Fingerprint(loaded) == Fingerprint(s));
    REQUIRE(loaded.spawns.size() == s.spawns.size());
    CHECK(loaded.spawns[0].cell == s.spawns[0].cell);
    CHECK(loaded.spawns[0].round == s.spawns[0].round);
    j["version"] = 1;
    CHECK_FALSE(LoadMatch(j, loaded, error));
}
