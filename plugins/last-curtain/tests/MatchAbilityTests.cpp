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

    // Makes the given unit the one whose turn it is.
    void TurnOf(MatchState &s, int unit)
    {
        s.turn = static_cast<int>(std::ranges::find(s.order, unit) - s.order.begin());
        s.now = {s.units[U(unit)].cell, static_cast<std::int8_t>(s.units[U(unit)].move), false};
    }

    int UnitOf(const MatchDef &def, const MatchState &s, const char *id, int faction)
    {
        for (int i = 0; i < static_cast<int>(s.units.size()); ++i)
            if (s.units[U(i)].kind == 0 && s.units[U(i)].faction == faction && def.characters[U(s.units[U(i)].def)].id == id)
                return i;
        return -1;
    }

    int SlotOf(const MatchDef &def, const Unit &u, const char *ability)
    {
        for (int i = 0; i < maxAbilities; ++i)
            if (u.abilities[U(i)] >= 0 && def.abilities[U(u.abilities[U(i)])].id == ability)
                return i;
        return -1;
    }

    // A free square `steps` steps from `from`, or -1.
    int SquareAt(const MatchState &s, int from, int steps)
    {
        for (int c = 0; c < static_cast<int>(s.board.cells.size()); ++c)
            if (Steps(s.board, from, c) == steps && std::ranges::none_of(s.units, [&](const Unit &u) { return u.cell == c; }))
                return c;
        return -1;
    }
}

TEST_CASE("Levelling learns the job's ability, or asks for a choice (T292)", "[match][levels]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int me = UnitOf(def, s, "bao", 0);
    TurnOf(s, me);
    Unit &u = s.units[U(me)];
    const int maxHp = u.maxHp;
    u.items[0] = I8(MatchDef::Find(def.items, "programme")); // 3 XP = level 2
    REQUIRE(Apply(def, s, {ActionKind::UseItem, 0}));
    CHECK(u.level == 2);
    CHECK(u.maxHp == maxHp + def.rules.levels.hp);
    CHECK(Current(s) == me);
    const auto legal = Legal(def, s);
    CHECK(legal == std::vector<Action>{{ActionKind::LevelUp, 0}, {ActionKind::LevelUp, 1}});
    CHECK(Check(def, s, {ActionKind::EndTurn}) == Why::MustChoose);
    REQUIRE(Apply(def, s, {ActionKind::LevelUp, 1}));
    CHECK(SlotOf(def, u, "gamble") >= 0); // attacker level 2: cleave or gamble
    CHECK(SlotOf(def, u, "cleave") < 0);
    CHECK(u.pending == -1);

    // A downed character's waiting choice is still asked first.
    MatchState t = Start(def);
    const int other = UnitOf(def, t, "khang", 0);
    t.units[U(other)].cell = -1;
    t.units[U(other)].back = 1;
    t.units[U(other)].pending = I8(static_cast<int>(std::ranges::find_if(def.jobs, [](const JobLevel &j) { return j.role == Role::Defender && j.choice >= 0; }) - def.jobs.begin()));
    CHECK(Current(t) == other);
    REQUIRE(Apply(def, t, {ActionKind::LevelUp, 0}));
    CHECK(Current(t) == t.order[0]);
}

TEST_CASE("Board abilities: Hit at range, Heal an ally at the healer's cost (T293)", "[match][abilities]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int me = UnitOf(def, s, "bao", 0), foe = 3; // units 3-5 are side 1
    TurnOf(s, me);
    const int snipe = SlotOf(def, s.units[U(me)], "snipe"); // range 3, cost 3, power 4
    REQUIRE(snipe >= 0);
    const int near = SquareAt(s, s.units[U(me)].cell, 2), far = SquareAt(s, s.units[U(me)].cell, 6);
    REQUIRE(near >= 0);
    REQUIRE(far >= 0);
    s.units[U(foe)].cell = static_cast<std::int16_t>(far);
    CHECK(Check(def, s, {ActionKind::BoardAbility, I8(snipe), I8(foe)}) == Why::OutOfRange);
    s.units[U(foe)].cell = static_cast<std::int16_t>(near);
    CHECK(Check(def, s, {ActionKind::BoardAbility, I8(snipe), I8(UnitOf(def, s, "khang", 0))}) == Why::OwnSide);
    s.units[U(me)].hp = 3;
    CHECK(Check(def, s, {ActionKind::BoardAbility, I8(snipe), I8(foe)}) == Why::TooCostly);
    s.units[U(me)].hp = 10;
    const int dealt = Damage(def, s, s.units[U(me)].abilities[U(snipe)], me, foe, 0);
    const int before = s.units[U(foe)].hp;
    CHECK(Preview(def, s, {ActionKind::BoardAbility, I8(snipe), I8(foe)}).dealtMin == dealt);
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, I8(snipe), I8(foe)}));
    CHECK(s.units[U(foe)].hp == before - dealt);
    CHECK(s.units[U(me)].hp == 7);
    CHECK(s.now.acted);

    // Mai's Tend: an ally in range gains 6, Mai pays 3.
    MatchState t = Start(def);
    const int mai = UnitOf(def, t, "mai", 0), ally = UnitOf(def, t, "khang", 0);
    TurnOf(t, mai);
    const int tend = SlotOf(def, t.units[U(mai)], "tend");
    t.units[U(ally)].hp = 10;
    const int maiHp = t.units[U(mai)].hp;
    t.units[U(ally)].cell = static_cast<std::int16_t>(SquareAt(t, t.units[U(mai)].cell, 2));
    REQUIRE(Apply(def, t, {ActionKind::BoardAbility, I8(tend), I8(ally)}));
    CHECK(t.units[U(ally)].hp == 16);
    CHECK(t.units[U(mai)].hp == maiHp - 3);
}

TEST_CASE("Items heal and grant XP (T294)", "[match][items]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int me = Current(s);
    Unit &u = s.units[U(me)];
    u.hp = 5;
    u.items[2] = I8(MatchDef::Find(def.items, "bandage"));
    CHECK(Preview(def, s, {ActionKind::UseItem, 2}).healed == 6);
    REQUIRE(Apply(def, s, {ActionKind::UseItem, 2}));
    CHECK(u.hp == 11);
    CHECK(u.items[2] == -1);
    CHECK(Check(def, s, {ActionKind::UseItem, 2}) == Why::AlreadyActed);
    MatchState t = Start(def);
    const int me2 = Current(t);
    t.units[U(me2)].items[0] = I8(MatchDef::Find(def.items, "programme"));
    CHECK(Check(def, t, {ActionKind::UseItem, 1}) == Why::NothingHere);
    REQUIRE(Apply(def, t, {ActionKind::UseItem, 0}));
    CHECK(t.units[U(me2)].xp == 3);
}

TEST_CASE("An event's choice rolls its chance and resolves the room (T295)", "[match][events]")
{
    MatchDef def = Data();
    const int applause = MatchDef::Find(def.events, "applause"); // success essence:2, failure hp:-3
    for (int chance : {100, 0})
    {
        def.events[U(applause)].choices[0].chance = chance;
        MatchState s = Start(def);
        const int me = Current(s);
        const int c = s.units[U(me)].cell;
        CHECK(Check(def, s, {ActionKind::Answer, 0}) == Why::NothingHere);
        s.board.cells[U(c)].event = I8(applause);
        const int hp = s.units[U(me)].hp;
        REQUIRE(Apply(def, s, {ActionKind::Answer, 0}));
        if (chance == 100)
            CHECK(s.units[U(me)].essence == 2);
        else
            CHECK(s.units[U(me)].hp == hp - 3);
        CHECK(s.board.cells[U(c)].resolved);
        CHECK(Check(def, s, {ActionKind::Answer, 0}) == Why::AlreadyActed);
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    }
}
