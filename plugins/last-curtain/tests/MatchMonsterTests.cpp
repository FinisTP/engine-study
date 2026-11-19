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

    bool Taken(const MatchState &s, int c) { return std::ranges::any_of(s.units, [&](const Unit &u) { return u.cell == c; }); }

    // A free square `steps` from `from`, of a kind other than a room.
    int SquareAt(const MatchState &s, int from, int steps)
    {
        for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
            if (Steps(s.board, from, c) == steps && !Taken(s, c) && s.board.cells[U(c)].kind != CellKind::Room)
                return c;
        return -1;
    }

    int Nearest(const MatchState &s, int cell)
    {
        int best = 1 << 20;
        for (const Unit &u : s.units)
            if (u.kind == 0 && u.cell >= 0)
                best = std::min(best, Steps(s.board, cell, u.cell));
        return best;
    }

    // A monster unit of this id, put on the board as a new unit.
    int Add(const MatchDef &def, MatchState &s, const char *id, int cell)
    {
        const int m = MatchDef::Find(def.monsters, id);
        Unit u;
        u.kind = 1;
        u.def = I8(m);
        u.cell = u.home = static_cast<std::int16_t>(cell);
        u.hp = u.maxHp = static_cast<std::int16_t>(def.monsters[U(m)].hp);
        u.speed = static_cast<std::int16_t>(def.monsters[U(m)].speed);
        u.attack = static_cast<std::int16_t>(def.monsters[U(m)].attack);
        u.abilities[0] = I8(def.monsters[U(m)].abilities[0]);
        s.units.push_back(u);
        return static_cast<int>(s.units.size()) - 1;
    }

    int GuardianOf(const MatchDef &def, const MatchState &s, int faction)
    {
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[U(i)].kind == 1 && s.units[U(i)].faction == faction && def.monsters[U(s.units[U(i)].def)].tier == Tier::Guardian)
                return i;
        return -1;
    }
}

TEST_CASE("Monsters act after each round by their behaviour (T299)", "[match][monsters]")
{
    MatchDef def = Data();
    def.monsters[U(MatchDef::Find(def.monsters, "stagehand"))].behaviour = Behaviour::Flee;
    MatchState s = Start(def);
    const int me = s.order[0];
    const int near = SquareAt(s, s.units[U(me)].cell, 2);
    const int spider = Add(def, s, "stagehand", near);
    const int before = Nearest(s, near);
    RunMonsters(def, s);
    CHECK(Nearest(s, s.units[U(spider)].cell) > before);

    MatchState t = Start(def);
    const int start = SquareAt(t, t.stage, 2);
    const int prompter = Add(def, t, "prompter", start); // speed 3, goes for the stage but never onto it
    RunMonsters(def, t);
    CHECK(Steps(t.board, t.units[U(prompter)].cell, t.stage) == 1);
}

TEST_CASE("A guardian strikes enemies next to it, and its own side cannot attack it (T300)", "[match][monsters]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int g1 = GuardianOf(def, s, 1);
    REQUIRE(g1 >= 0);
    const int me = s.order[0];
    const int enemyOfG1 = s.units[U(me)].faction == 0 ? me : 0;
    int beside = -1;
    for (int d = 0; d < 4; ++d)
        if (const int n = Neighbour(s.board, s.units[U(g1)].cell, d); n >= 0 && !Taken(s, n))
            beside = n;
    REQUIRE(beside >= 0);
    s.units[U(enemyOfG1)].cell = static_cast<std::int16_t>(beside);
    s.units[U(enemyOfG1)].hp = 40;
    const int expected = 40 - Damage(def, s, s.units[U(g1)].abilities[0], g1, enemyOfG1, 0);
    RunMonsters(def, s);
    CHECK(s.units[U(enemyOfG1)].hp == expected);

    // Side 1 cannot attack its own guardian; side 0 cannot step into side 1's hub while it stands.
    MatchState t = Start(def);
    const int g = GuardianOf(def, t, t.units[U(t.order[0])].faction);
    const int mine = t.order[0];
    for (int d = 0; d < 4; ++d)
        if (const int n = Neighbour(t.board, t.units[U(g)].cell, d); n >= 0 && !Taken(t, n))
            t.units[U(mine)].cell = static_cast<std::int16_t>(n);
    CHECK(Check(def, t, {ActionKind::Attack, I8(g)}) == Why::OwnSide);
}

namespace
{
    // The shipped data with every centre square spawning `id` from round 1.
    MatchDef CentreOf(const char *id)
    {
        MatchDef def = Data();
        for (SpawnRow &w : def.spawns)
            if (w.zone == 2)
                w.monster = MatchDef::Find(def.monsters, id);
        return def;
    }

    int SpawnedOn(const MatchState &s, int home)
    {
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[U(i)].kind == 1 && s.units[U(i)].home == home)
                return i;
        return -1;
    }
}

TEST_CASE("A 3x3 monster covers nine squares, never a wall, the stage or another unit (T315)", "[match][monsters]")
{
    const MatchDef def = CentreOf("the-spider");
    MatchState s = Start(def);
    for (int home : s.board.centreSpawns)
    {
        const int m = SpawnedOn(s, home);
        REQUIRE(m >= 0);
        const Unit &spider = s.units[U(m)];
        REQUIRE(spider.cell >= 0);
        const std::vector<int> squares = Squares(s.board, spider);
        REQUIRE(squares.size() == 9);
        CHECK(std::ranges::find(squares, home) != squares.end());
        for (int c : squares)
        {
            CHECK(s.board.cells[U(c)].kind != CellKind::Wall);
            CHECK(c != s.stage);
            CHECK(Covers(s.board, spider, c));
            for (int o = 0; o < static_cast<int>(s.units.size()); ++o)
                if (o != m && s.units[U(o)].hp > 0)
                    CHECK_FALSE(Covers(s.board, s.units[U(o)], c));
        }
        // 1. A character cannot step onto any of its squares.
        for (int c : squares)
            for (int d = 0; d < 4; ++d)
                if (const int n = Neighbour(s.board, c, d); n >= 0 && std::ranges::find(squares, n) == squares.end() && !Taken(s, n))
                {
                    const int me = Current(s);
                    s.units[U(me)].cell = s.now.origin = static_cast<std::int16_t>(n);
                    CHECK(Check(def, s, {ActionKind::Move, I8((d + 2) % 4)}) == Why::Blocked);
                }
    }

    // 2. In a lane den (a room among walls) it never fits, so it never appears.
    MatchDef lanes = Data();
    for (SpawnRow &w : lanes.spawns)
        if (w.zone == 0)
            w.monster = MatchDef::Find(lanes.monsters, "the-spider");
    const MatchState t = Start(lanes);
    for (int den : t.board.weakDens)
        CHECK(t.units[U(SpawnedOn(t, den))].cell == -1);
}

TEST_CASE("Anyone next to any of its squares can attack it, and its blows reach from any (T316)", "[match][monsters]")
{
    MatchDef def = CentreOf("the-spider");
    MatchState s = Start(def);
    const int m = SpawnedOn(s, s.board.centreSpawns[1]);
    const std::vector<int> squares = Squares(s.board, s.units[U(m)]);
    // 1. The square below its bottom-right corner touches only that corner.
    const int corner = squares.back(), below = Neighbour(s.board, corner, 2);
    REQUIRE(below >= 0);
    const int me = Current(s);
    s.units[U(me)].cell = s.now.origin = static_cast<std::int16_t>(below);
    CHECK(Check(def, s, {ActionKind::Attack, I8(m)}) == Why::Ok);
    const int away = Neighbour(s.board, below, 2);
    REQUIRE(away >= 0);
    s.units[U(me)].cell = s.now.origin = static_cast<std::int16_t>(away);
    CHECK(Check(def, s, {ActionKind::Attack, I8(m)}) == Why::OutOfRange);

    // 2. As a guard it strikes whoever stands beside any of its squares.
    def.monsters[U(MatchDef::Find(def.monsters, "the-spider"))].behaviour = Behaviour::Guard;
    MatchState t = Start(def);
    const int g = SpawnedOn(t, t.board.centreSpawns[1]);
    const int victim = Current(t);
    t.units[U(victim)].cell = static_cast<std::int16_t>(Neighbour(t.board, Squares(t.board, t.units[U(g)]).back(), 2));
    t.units[U(victim)].hp = 90;
    RunMonsters(def, t);
    CHECK(t.units[U(victim)].hp < 90);
}
