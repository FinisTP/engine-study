#include "MatchTestData.hpp"

#include <Match.hpp>

#include <algorithm>
#include <array>

using namespace umbra::match;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }
    std::int8_t I8(int i) { return static_cast<std::int8_t>(i); }

    MatchState Start(const MatchDef &def)
    {
        return NewMatch(def, 7, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
    }

    // Every monster on the board takes its turn, in unit order (what the old monsters' phase did).
    void ActAll(const MatchDef &def, MatchState &s)
    {
        for (int i = 0; i < static_cast<int>(s.units.size()) && !s.battle.on; ++i)
            if (s.units[U(i)].kind == 1 && Up(s.units[U(i)]))
                MonsterAct(def, s, i);
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

TEST_CASE("Monsters act by their behaviour on their turn (T299)", "[match][monsters]")
{
    MatchDef def = Data();
    def.monsters[U(MatchDef::Find(def.monsters, "stagehand"))].behaviour = Behaviour::Flee;
    MatchState s = Start(def);
    const int me = s.order[0];
    const int near = SquareAt(s, s.units[U(me)].cell, 2);
    const int spider = Add(def, s, "stagehand", near);
    const int before = Nearest(s, near);
    ActAll(def, s);
    CHECK(Nearest(s, s.units[U(spider)].cell) > before);

    MatchState t = Start(def);
    const int start = SquareAt(t, t.stage, 2);
    const int prompter = Add(def, t, "prompter", start); // speed 3, goes for the stage but never onto it
    ActAll(def, t);
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
    ActAll(def, s);
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
    int away = -1;
    for (int d = 0; d < 4 && away < 0; ++d)
        if (const int n = Neighbour(s.board, below, d);
            n >= 0 && std::ranges::none_of(squares, [&](int q) { return n == q || std::ranges::any_of(std::array{0, 1, 2, 3}, [&](int e) { return Neighbour(s.board, q, e) == n; }); }))
            away = n;
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
    ActAll(def, t);
    CHECK(t.units[U(victim)].hp < 90);
}

namespace
{
    bool InCentre(const MatchState &s, const Unit &u)
    {
        return std::ranges::all_of(Squares(s.board, u), [&](int c) { return s.board.cells[U(c)].kind == CellKind::Centre; });
    }

    // A free centre square `steps` from the hunter, away from the other hunter.
    int CentreSquare(const MatchState &s, int hunter, int steps)
    {
        for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
            if (s.board.cells[U(c)].kind == CellKind::Centre && c != s.stage && !Taken(s, c) && Steps(s.board, s.units[U(hunter)].cell, c) == steps)
                return c;
        return -1;
    }
}

TEST_CASE("A hunter walks to a character it sees, never out of the centre (T317)", "[match][monsters][hunt]")
{
    MatchDef def = Data();
    MatchState s = Start(def);
    const int a = SpawnedOn(s, s.board.centreSpawns[0]), b = SpawnedOn(s, s.board.centreSpawns[1]);
    REQUIRE(def.monsters[U(s.units[U(a)].def)].behaviour == Behaviour::Hunt);
    s.units[U(b)].cell = -1, s.units[U(b)].hp = 0; // one hunter only
    // 1. In sight, three steps away: it walks beside the character and starts a battle as attacker.
    const int me = s.order[0];
    s.units[U(me)].cell = static_cast<std::int16_t>(CentreSquare(s, a, 3));
    s.units[U(me)].hp = 99;
    ActAll(def, s);
    CHECK(InCentre(s, s.units[U(a)]));
    REQUIRE(s.battle.on);
    CHECK(s.battle.attacker == a);
    CHECK(s.battle.defender == me);
    CHECK(Current(s) == me);

    // 2. A character outside the centre: it comes closer but never leaves it.
    def.monsters[U(s.units[U(a)].def)].sight = 9;
    MatchState t = Start(def);
    t.units[U(SpawnedOn(t, t.board.centreSpawns[1]))].cell = -1;
    t.units[U(SpawnedOn(t, t.board.centreSpawns[1]))].hp = 0;
    const int h = SpawnedOn(t, t.board.centreSpawns[0]);
    int far = -1;
    for (int c = 0; c < static_cast<int>(t.board.cells.size()) && far < 0; ++c)
    {
        const CellKind k = t.board.cells[U(c)].kind;
        if (k == CellKind::Wall || k == CellKind::Centre || Taken(t, c) || Steps(t.board, t.units[U(h)].cell, c) < 5 || Steps(t.board, t.units[U(h)].cell, c) > 7)
            continue;
        int nearestCentre = 99;
        for (int z = 0; z < static_cast<int>(t.board.cells.size()); ++z)
            if (t.board.cells[U(z)].kind == CellKind::Centre)
                nearestCentre = std::min(nearestCentre, Steps(t.board, z, c));
        if (nearestCentre >= 2)
            far = c;
    }
    REQUIRE(far >= 0);
    const int you = t.order[0];
    t.units[U(you)].cell = static_cast<std::int16_t>(far);
    const int before = Steps(t.board, t.units[U(h)].cell, far);
    ActAll(def, t);
    CHECK(InCentre(t, t.units[U(h)]));
    CHECK(Steps(t.board, t.units[U(h)].cell, far) < before);
    CHECK_FALSE(t.battle.on);

    // 3. Out of sight it stays.
    MatchState u = Start(Data());
    const int still = SpawnedOn(u, u.board.centreSpawns[0]);
    const int at = u.units[U(still)].cell;
    ActAll(Data(), u);
    CHECK(u.units[U(still)].cell == at);
}

TEST_CASE("A hunter's battle waits for the defender's picks, then the order goes on (T318)", "[match][monsters][hunt]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int a = SpawnedOn(s, s.board.centreSpawns[0]);
    int x = -1; // the last character in the order
    for (const std::int8_t u : s.order)
        if (s.units[U(u)].kind == 0)
            x = u;
    for (int u = 0; u < static_cast<int>(s.units.size()); ++u)
        if (s.units[U(u)].kind == 0 && u != x)
            s.units[U(u)].hp = s.units[U(u)].maxHp = 99;
    s.units[U(x)].cell = static_cast<std::int16_t>(CentreSquare(s, a, 2));
    s.units[U(x)].hp = 99;
    // 1. Characters end their turns until the hunter's turn comes and it starts a battle with X.
    for (int i = 0; i < 200 && !s.battle.on; ++i)
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    REQUIRE(s.battle.on);
    CHECK(s.battle.attacker == a);
    CHECK(s.battle.defender == x);
    CHECK(s.order[U(s.turn)] == a); // the battle is the hunter's turn
    CHECK(Current(s) == x);
    CHECK(Check(def, s, {ActionKind::EndTurn}) == Why::MustChoose);
    // 2. X picks to the end; then the order goes on after the hunter.
    const int place = s.turn, round = s.round;
    while (s.battle.on && s.battle.defender == x)
        REQUIRE(Apply(def, s, {ActionKind::BattleAbility, 0}));
    CHECK((s.round > round || s.turn > place));
    CHECK(Current(s) >= 0);
}

TEST_CASE("Monsters pick shields and heals when they help (T319)", "[match][monsters]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int foe = s.order[0];
    auto battle = [&](const char *id, int hp, int foeHp) {
        const int m = Add(def, s, id, -1);
        const MonsterDef &d = def.monsters[U(MatchDef::Find(def.monsters, id))];
        for (std::size_t i = 0; i < d.abilities.size(); ++i)
            s.units[U(m)].abilities[i] = I8(d.abilities[i]);
        s.units[U(m)].defence = static_cast<std::int16_t>(d.defence);
        s.units[U(m)].hp = static_cast<std::int16_t>(hp);
        s.units[U(foe)].hp = static_cast<std::int16_t>(foeHp);
        s.battle = Battle{};
        s.battle.on = true, s.battle.attacker = I8(foe), s.battle.defender = I8(m), s.battle.round = 1;
        s.battle.fighters = {I8(foe), -1, I8(m), -1};
        s.battle.order = {0, 2, -1, -1};
        s.battle.next = 1; // the monster's pick
        return m;
    };
    // 1. Ghost chorus (bite; mend): healthy it bites, below half HP it mends.
    CHECK(MonsterPick(def, s, battle("ghost-chorus", 30, 99)) == 0);
    CHECK(MonsterPick(def, s, battle("ghost-chorus", 10, 99)) == 1);
    // 2. Usherette (claw; brace): about to be downed, it braces; if its claw downs the foe first, it claws.
    CHECK(MonsterPick(def, s, battle("usherette", 16, 99)) == 0);
    CHECK(MonsterPick(def, s, battle("usherette", 3, 99)) == 1);
    CHECK(MonsterPick(def, s, battle("usherette", 3, 1)) == 0);
}
