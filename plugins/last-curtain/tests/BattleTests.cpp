#include "NightTestData.hpp"

#include <algorithm>

using namespace umbra::night;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }

    // A monster of this kind in the guest's cell, with these HP.
    int PutMonster(const NightDef &def, NightState &state, const char *kind, int cell, int hp)
    {
        const MonsterDef &m = def.monsters[U(NightDef::Find(def.monsters, kind))];
        state.monsters.push_back({static_cast<std::int8_t>(NightDef::Find(def.monsters, kind)), static_cast<std::int8_t>(cell),
                                  static_cast<std::int8_t>(state.round + 10), static_cast<std::int16_t>(hp),
                                  static_cast<std::int16_t>(m.might), static_cast<std::int16_t>(m.sanity)});
        return static_cast<int>(state.monsters.size()) - 1;
    }

    // Plays the current guest's battle: always strike the first target.
    void StrikeOut(const NightDef &def, NightState &state)
    {
        for (int i = 0; i < 100 && state.battle.active; ++i)
        {
            const auto legal = LegalActions(def, state);
            const auto strike = std::find_if(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::Strike; });
            REQUIRE(strike != legal.end());
            REQUIRE(Apply(def, state, *strike));
        }
    }

    NightDef Quiet()
    {
        NightDef def = Data();
        def.rules.monsters.chancePercent = 0; // no monster appears by itself
        return def;
    }

    Fighter AsGuest(int g) { Fighter f; f.guest = static_cast<std::int8_t>(g); return f; }
    Fighter AsMonster(int m) { Fighter f; f.monster = static_cast<std::int8_t>(m); f.side = 1; return f; }
}

TEST_CASE("Damage follows the data formula for both types (T98)", "[night][battle]")
{
    NightDef def = Quiet();
    NightState state = NewNight(def, 1);
    const int khang = NightDef::Find(def.guests, "khang"), thanh = NightDef::Find(def.guests, "thanh");
    const int m = PutMonster(def, state, "stagehand", state.foyer, 10);
    const int might = state.monsters[U(m)].might, sanity = state.monsters[U(m)].sanity;
    // Khang hits physically; the stagehand resists physical: halved.
    CHECK(Damage(def, state, AsGuest(khang), AsMonster(m)) == std::max(1, 3 + 7 - might / 2) / 2);
    // Thanh hits magically: no resistance.
    CHECK(Damage(def, state, AsGuest(thanh), AsMonster(m)) == std::max(1, 3 + 7 - sanity / 2));
    def.rules.battle.base = 5;
    CHECK(Damage(def, state, AsGuest(thanh), AsMonster(m)) == std::max(1, 5 + 7 - sanity / 2));
    // Never below minDamage.
    CHECK(Damage(def, state, AsMonster(m), AsGuest(khang)) >= def.rules.battle.minDamage);
}

TEST_CASE("The starter strikes first and a battle lasts three rounds (T98)", "[night][battle]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 1);
    const int g = Current(state);
    const int m = PutMonster(def, state, "moths", state.foyer, 999);
    const std::size_t since = state.logCount;
    REQUIRE(Apply(def, state, {ActionKind::Attack, static_cast<std::int8_t>(m)}));
    REQUIRE(state.battle.active);
    CHECK(Current(state) == g);
    StrikeOut(def, state);
    int hits = 0, first = -2;
    for (const LogEntry &e : NewLog(state, since))
        if (e.kind == LogKind::Hit)
        {
            if (first == -2)
                first = e.who;
            ++hits;
        }
    CHECK(first == g);
    CHECK(hits == 2 * def.rules.battle.rounds);
    CHECK_FALSE(state.battle.active);
    REQUIRE(state.monsters.size() == 1);
    CHECK(state.monsters[0].hp < 999);
    CHECK(Current(state) != g); // the board turn is over
}

TEST_CASE("Buffs show their effect and run out (T98)", "[night][battle]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 1);
    const int g = Current(state);
    Guest &guest = state.guests[U(g)];
    guest.hp = guest.maxHp = 100;
    guest.items[0] = static_cast<std::int8_t>(NightDef::Find(def.items, "coat"));
    guest.items[1] = static_cast<std::int8_t>(NightDef::Find(def.items, "whetstone"));
    const int m = PutMonster(def, state, "stagehand", state.foyer, 999);
    REQUIRE(Apply(def, state, {ActionKind::Attack, static_cast<std::int8_t>(m)}));
    const int monsterFighter = state.battle.fighters[0].guest == g ? 1 : 0, guestFighter = 1 - monsterFighter;

    // Guarded: the monster's next hit (right after the guest's action) is halved, the one after is not.
    const int full = Damage(def, state, state.battle.fighters[U(monsterFighter)], state.battle.fighters[U(guestFighter)]);
    const int hp = guest.hp;
    REQUIRE(Apply(def, state, {ActionKind::UseItem, 0}));
    CHECK(hp - guest.hp == std::max(def.rules.battle.minDamage, full / def.rules.battle.guardDivisor));
    CHECK(Damage(def, state, state.battle.fighters[U(monsterFighter)], state.battle.fighters[U(guestFighter)]) >= full);
    // The stagehand's hit left the guest Weakened: -2 Might for 2 rounds.
    const int weakMight = Stat(def, state, g, StatKind::Might);
    CHECK(weakMight == guest.might - 2);
    // Whetstone: +2 Might for the battle.
    REQUIRE(Apply(def, state, {ActionKind::UseItem, 1}));
    CHECK(Stat(def, state, g, StatKind::Might) >= weakMight + 2);
    StrikeOut(def, state);
    CHECK(Stat(def, state, g, StatKind::Might) == guest.might); // the battle is over, buffs with it
}

TEST_CASE("Killing monsters gives XP and levels without healing (T98)", "[night][battle]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 1);
    const int g = Current(state);
    Guest &guest = state.guests[U(g)];
    for (int kill = 0; kill < def.rules.levels.xp[0]; ++kill)
    {
        while (Current(state) != g)
            REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
        const int m = PutMonster(def, state, "moths", guest.cell, 1);
        REQUIRE(Apply(def, state, {ActionKind::Attack, static_cast<std::int8_t>(m)}));
        StrikeOut(def, state);
        CHECK(state.monsters.empty());
    }
    CHECK(guest.xp == def.rules.levels.xp[0]);
    REQUIRE(Current(state) == g);
    const auto legal = LegalActions(def, state);
    REQUIRE(legal.size() == 2);
    CHECK(std::all_of(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::LevelUp; }));
    const int hp = guest.hp, maxHp = guest.maxHp, might = guest.might;
    REQUIRE(Apply(def, state, {ActionKind::LevelUp, 0}));
    CHECK(guest.level == 2);
    CHECK(guest.might == might + def.rules.levels.stat);
    CHECK(guest.maxHp == maxHp + def.rules.levels.hp);
    CHECK(guest.hp == hp);
}

TEST_CASE("Monsters appear, grow with the hour, leave, and force a fight when met (T98)", "[night][battle]")
{
    NightDef def = Data();
    def.rules.monsters.chancePercent = 100;
    NightState state = NewNight(def, 9);
    // Open some rooms so monsters have somewhere to be.
    for (int i = 0; i < 6 && Current(state) >= 0; ++i)
    {
        const auto legal = LegalActions(def, state);
        const auto move = std::find_if(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::Move; });
        REQUIRE(Apply(def, state, move != legal.end() ? *move : Action{ActionKind::EndTurn}));
    }
    int seenGrown = 0;
    while (Current(state) >= 0 && state.round <= 8)
    {
        if (state.battle.active)
        {
            StrikeOut(def, state);
            continue;
        }
        const auto legal = LegalActions(def, state);
        if (legal[0].kind == ActionKind::LevelUp || legal[0].kind == ActionKind::TakeOmen)
        {
            REQUIRE(Apply(def, state, legal[0]));
            continue;
        }
        if (state.round < def.rules.monsters.fromRound)
            CHECK(state.monsters.empty());
        CHECK(static_cast<int>(state.monsters.size()) <= def.rules.monsters.max);
        for (const Monster &m : state.monsters)
        {
            CHECK(m.leavesRound > state.round);
            CHECK(m.leavesRound <= state.round + def.rules.monsters.stayRounds);
            const MonsterDef &md = def.monsters[U(m.kind)];
            seenGrown += m.hp > md.hp ? 1 : 0;
        }
        REQUIRE(Apply(def, state, {ActionKind::EndTurn}));
    }
    CHECK(seenGrown > 0);

    // Walking into a monster's room starts a battle at once, the monster's side first.
    NightDef plain = Quiet();
    for (RoomDef &room : plain.rooms)
        if (room.kind != RoomKind::Foyer && room.kind != RoomKind::UnderStage)
            room.kind = RoomKind::Plain, room.doors = 15;
    NightState walk = NewNight(plain, 2);
    const int g = Current(walk);
    REQUIRE(Apply(plain, walk, {ActionKind::Move, 0})); // north out of the foyer
    PutMonster(plain, walk, "moths", walk.foyer, 999);
    REQUIRE(Apply(plain, walk, {ActionKind::Move, 2})); // and back
    REQUIRE(walk.battle.active);
    CHECK(walk.battle.fighters[0].monster >= 0);
    CHECK(walk.guests[U(g)].cell == walk.foyer);
}

TEST_CASE("Leaving is not possible in the first round (T98)", "[night][battle]")
{
    const NightDef def = Quiet();
    NightState state = NewNight(def, 1);
    const int m = PutMonster(def, state, "moths", state.foyer, 999);
    REQUIRE(Apply(def, state, {ActionKind::Attack, static_cast<std::int8_t>(m)}));
    CHECK_FALSE(Apply(def, state, {ActionKind::Leave}));
    const auto legal = LegalActions(def, state);
    REQUIRE(Apply(def, state, *std::find_if(legal.begin(), legal.end(), [](const Action &a) { return a.kind == ActionKind::Strike; })));
    REQUIRE(state.battle.round == 2);
    REQUIRE(Apply(def, state, {ActionKind::Leave}));
    CHECK_FALSE(state.battle.active);
    CHECK(state.monsters.size() == 1);
}
