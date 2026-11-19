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
