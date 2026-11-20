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

TEST_CASE("Levelling deals three cards to choose from, before anything else (T292)", "[match][levels]")
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
    REQUIRE(s.offer.size() == 3);
    CHECK(Legal(def, s) == std::vector<Action>{{ActionKind::LevelUp, 0}, {ActionKind::LevelUp, 1}, {ActionKind::LevelUp, 2}});
    CHECK(Check(def, s, {ActionKind::EndTurn}) == Why::MustChoose);
    CHECK(Check(def, s, {ActionKind::LevelUp, 3}) == Why::NothingHere);
    REQUIRE(Apply(def, s, {ActionKind::LevelUp, 1}));
    CHECK(u.pending == -1);
    CHECK(s.offer.empty());

    // A downed character's waiting choice is still asked first.
    MatchState t = Start(def);
    const int other = UnitOf(def, t, "khang", 0);
    t.units[U(other)].cell = -1;
    t.units[U(other)].back = 1;
    t.units[U(other)].pending = 0;
    t.offer = {{1, 1, 1}, {1, 2, 1}, {1, 0, 4}};
    CHECK(Current(t) == other);
    REQUIRE(Apply(def, t, {ActionKind::LevelUp, 0}));
    CHECK(Current(t) == t.order[0]);
}

TEST_CASE("Level cards: learn a role's ability, raise a stat, or upgrade a known ability (T348)", "[match][levels]")
{
    const MatchDef def = Data();
    REQUIRE_FALSE(def.levelCards.empty());
    MatchState base = Start(def);
    const int me = UnitOf(def, base, "bao", 0);
    TurnOf(base, me);
    const int jab = SlotOf(def, base.units[U(me)], "jab"), snipe = SlotOf(def, base.units[U(me)], "snipe");
    auto offered = [&](LevelCard card) {
        MatchState s = base;
        s.units[U(me)].pending = 0;
        s.offer = {card};
        REQUIRE(Apply(def, s, {ActionKind::LevelUp, 0}));
        return s;
    };
    // 1. Learn: the ability goes in a free slot.
    const int cleave = MatchDef::Find(def.abilities, "cleave");
    MatchState l = offered({0, I8(cleave), 0});
    CHECK(SlotOf(def, l.units[U(me)], "cleave") >= 0);
    // 2. A stat: attack +1 (HP raises both HP and its maximum).
    MatchState a = offered({1, 1, 1});
    CHECK(a.units[U(me)].attack == base.units[U(me)].attack + 1);
    MatchState h = offered({1, 0, 4});
    CHECK(h.units[U(me)].maxHp == base.units[U(me)].maxHp + 4);
    CHECK(h.units[U(me)].hp == base.units[U(me)].hp + 4);
    // 3. Upgrade power: the ability hits harder for this unit only.
    const int foe = 3;
    MatchState p = offered({2, I8(jab), 0});
    CHECK(Damage(def, p, p.units[U(me)].abilities[U(jab)], me, foe, 0) == Damage(def, base, base.units[U(me)].abilities[U(jab)], me, foe, 0) + def.rules.levels.upgradePower);
    // 4. Upgrade cost: the board ability costs less HP (and the forecast says so).
    MatchState c = offered({2, I8(snipe), 1});
    c.units[U(foe)].cell = static_cast<std::int16_t>(SquareAt(c, c.units[U(me)].cell, 2));
    CHECK(Preview(def, c, {ActionKind::BoardAbility, I8(snipe), I8(foe)}).cost == def.abilities[U(c.units[U(me)].abilities[U(snipe)])].hpCost - def.rules.levels.upgradeCost);
    const int before = c.units[U(me)].hp;
    REQUIRE(Apply(def, c, {ActionKind::BoardAbility, I8(snipe), I8(foe)}));
    CHECK(c.units[U(me)].hp == before - (def.abilities[U(c.units[U(me)].abilities[U(snipe)])].hpCost - def.rules.levels.upgradeCost));
    // 5. Offers over many seeds hold all three kinds, three distinct cards each.
    bool kinds[3] = {};
    for (std::uint64_t seed = 1; seed <= 40; ++seed)
    {
        MatchState s = NewMatch(def, seed, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
        const int m = Current(s);
        s.units[U(m)].items[0] = I8(MatchDef::Find(def.items, "programme"));
        s.units[U(m)].xp = static_cast<std::int16_t>(def.rules.levels.xp[0] - 3);
        REQUIRE(Apply(def, s, {ActionKind::UseItem, 0}));
        REQUIRE(s.offer.size() == 3);
        for (std::size_t i = 0; i < 3; ++i)
        {
            kinds[s.offer[i].kind] = true;
            for (std::size_t k = 0; k < i; ++k)
                CHECK_FALSE((s.offer[i].kind == s.offer[k].kind && s.offer[i].a == s.offer[k].a && s.offer[i].b == s.offer[k].b));
        }
    }
    CHECK(kinds[0]);
    CHECK(kinds[1]);
    CHECK(kinds[2]);
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

TEST_CASE("Board abilities are told and teach: a heal is logged, both grant abilityXp (T351)", "[match][abilities]")
{
    const MatchDef def = Data();
    REQUIRE(def.rules.levels.abilityXp > 0);
    // 1. Mai's Tend: logged as Healed with the amount; Mai gains abilityXp.
    MatchState t = Start(def);
    const int mai = UnitOf(def, t, "mai", 0), ally = UnitOf(def, t, "khang", 0);
    TurnOf(t, mai);
    t.units[U(ally)].hp = 10;
    t.units[U(ally)].cell = static_cast<std::int16_t>(SquareAt(t, t.units[U(mai)].cell, 2));
    const int xp = t.units[U(mai)].xp;
    const std::size_t logged = t.log.size();
    REQUIRE(Apply(def, t, {ActionKind::BoardAbility, I8(SlotOf(def, t.units[U(mai)], "tend")), I8(ally)}));
    bool healed = false;
    for (std::size_t i = logged; i < t.log.size(); ++i)
        healed = healed || (t.log[i].kind == LogKind::Healed && t.log[i].a == mai && t.log[i].b == ally && t.log[i].c == 6);
    CHECK(healed);
    CHECK(t.units[U(mai)].xp == xp + def.rules.levels.abilityXp);
    // 2. Bao's Snipe on a foe: abilityXp too.
    MatchState s = Start(def);
    const int me = UnitOf(def, s, "bao", 0), foe = 3;
    TurnOf(s, me);
    s.units[U(foe)].cell = static_cast<std::int16_t>(SquareAt(s, s.units[U(me)].cell, 2));
    s.units[U(foe)].hp = 90;
    const int baoXp = s.units[U(me)].xp;
    REQUIRE(Apply(def, s, {ActionKind::BoardAbility, I8(SlotOf(def, s.units[U(me)], "snipe")), I8(foe)}));
    CHECK(s.units[U(me)].xp == baoXp + def.rules.levels.abilityXp);
}

TEST_CASE("A heal never goes to someone already at full HP (T353)", "[match][abilities]")
{
    const MatchDef def = Data();
    MatchState s = Start(def);
    const int mai = UnitOf(def, s, "mai", 0), ally = UnitOf(def, s, "khang", 0);
    TurnOf(s, mai);
    s.units[U(ally)].cell = static_cast<std::int16_t>(SquareAt(s, s.units[U(mai)].cell, 2));
    s.units[U(ally)].hp = s.units[U(ally)].maxHp;
    const Action tend{ActionKind::BoardAbility, I8(SlotOf(def, s.units[U(mai)], "tend")), I8(ally)};
    CHECK(Check(def, s, tend) == Why::FullHp);
    const std::vector<Action> legal = Legal(def, s);
    CHECK(std::ranges::find(legal, tend) == legal.end());
    s.units[U(ally)].hp = static_cast<std::int16_t>(s.units[U(ally)].maxHp - 1);
    CHECK(Check(def, s, tend) == Why::Ok);
}

TEST_CASE("A straight ability counts squares across walls, a walking one counts steps through doors (T354)", "[match][abilities]")
{
    const MatchDef def = Data();
    const AbilityDef &snipe = def.abilities[U(MatchDef::Find(def.abilities, "snipe"))];
    REQUIRE(snipe.reach == RangeBy::Straight);
    REQUIRE(def.abilities[U(MatchDef::Find(def.abilities, "bash"))].reach == RangeBy::Walk);
    MatchState s = Start(def);
    const int me = UnitOf(def, s, "bao", 0), foe = 3;
    TurnOf(s, me);
    // 1. A free open square within snipe's range in a straight count but farther by the doors.
    const Board &b = s.board;
    const int from = s.units[U(me)].cell;
    int far = -1;
    for (int c = 0; c < static_cast<int>(b.cells.size()) && far < 0; ++c)
    {
        const int manhattan = std::abs(c % b.width - from % b.width) + std::abs(c / b.width - from / b.width);
        if (b.cells[U(c)].kind != CellKind::Wall && manhattan <= snipe.range && Steps(b, from, c) > snipe.range &&
            std::ranges::none_of(s.units, [&](const Unit &u) { return u.cell == c; }))
            far = c;
    }
    REQUIRE(far >= 0);
    s.units[U(foe)].cell = static_cast<std::int16_t>(far);
    s.units[U(foe)].hp = 90;
    CHECK(Check(def, s, {ActionKind::BoardAbility, I8(SlotOf(def, s.units[U(me)], "snipe")), I8(foe)}) == Why::Ok);
}
