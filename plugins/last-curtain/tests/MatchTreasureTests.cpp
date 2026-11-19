#include "MatchTestData.hpp"

#include <Match.hpp>
#include <MatchBots.hpp>

#include <algorithm>

using namespace umbra::match;

namespace
{
    std::size_t U(int i) { return static_cast<std::size_t>(i); }

    // The first character to act, standing on an item room's treasure; returns the floor index.
    int OnTreasure(const MatchDef &def, MatchState &s)
    {
        s = NewMatch(def, 7, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
        for (int f = 0; f < static_cast<int>(s.floor.size()); ++f)
            if (s.floor[U(f)].treasure)
            {
                s.units[U(Current(s))].cell = s.now.origin = s.floor[U(f)].cell;
                return f;
            }
        return -1;
    }
}

TEST_CASE("An item room deals 2-5 face-down cards; the opener must pick one and gets its reward (T345)", "[match][treasure]")
{
    const MatchDef def = Data();
    REQUIRE_FALSE(def.treasure.empty());
    CHECK(def.rules.treasure.min == 2);
    CHECK(def.rules.treasure.max == 5);
    MatchState s;
    const int f = OnTreasure(def, s);
    REQUIRE(f >= 0);
    const int me = Current(s);
    REQUIRE(Apply(def, s, {ActionKind::PickUp, static_cast<std::int8_t>(f)}));
    // 1. The cards are dealt; nothing else until one is chosen.
    REQUIRE(s.cards.size() >= 2);
    REQUIRE(s.cards.size() <= 5);
    CHECK(Current(s) == me);
    CHECK(Check(def, s, {ActionKind::EndTurn}) == Why::MustChoose);
    CHECK(Check(def, s, {ActionKind::Card, static_cast<std::int8_t>(s.cards.size())}) == Why::NothingHere);
    // 2. The chosen card's reward is given and the cards are gone.
    const TreasureDef &t = def.treasure[U(s.cards[0])];
    const Unit before = s.units[U(me)];
    REQUIRE(Apply(def, s, {ActionKind::Card, 0}));
    CHECK(s.cards.empty());
    const Unit &after = s.units[U(me)];
    switch (t.kind)
    {
    case TreasureKind::Item: CHECK(std::ranges::count(after.items, static_cast<std::int8_t>(t.item)) > std::ranges::count(before.items, static_cast<std::int8_t>(t.item))); break;
    case TreasureKind::Essence: CHECK(after.essence == before.essence + t.amount); break;
    case TreasureKind::Xp: CHECK((after.xp > before.xp || after.level > before.level)); break;
    case TreasureKind::Heal: CHECK(after.hp >= before.hp); break;
    }
    CHECK(std::ranges::any_of(s.log, [&](const LogEntry &e) { return e.kind == LogKind::Treasure && e.a == me; }));
    CHECK(s.now.acted);

    // 3. Bots open treasure and choose too.
    MatchState b;
    const int g = OnTreasure(def, b);
    REQUIRE(Apply(def, b, {ActionKind::PickUp, static_cast<std::int8_t>(g)}));
    REQUIRE(Apply(def, b, ChooseAction(def, b, BotLevel::Medium)));
    CHECK(b.cards.empty());
}

TEST_CASE("A claimed item or event room comes back later in a free room of the same half, never the centre (T347)", "[match][treasure]")
{
    const MatchDef def = Data();
    REQUIRE(def.rules.rooms.respawn >= 1);
    MatchState s;
    const int f = OnTreasure(def, s);
    const int cell = s.floor[U(f)].cell, half = cell / s.board.width > s.board.height / 2 ? 0 : 1;
    REQUIRE(Apply(def, s, {ActionKind::PickUp, static_cast<std::int8_t>(f)}));
    REQUIRE(Apply(def, s, {ActionKind::Card, 0}));
    // 1. Scheduled now: due respawn rounds on, in a room of the same half.
    REQUIRE(s.roomSpawns.size() == 1);
    const RoomSpawn r = s.roomSpawns[0];
    CHECK(r.kind == 1);
    CHECK(r.round == s.round + def.rules.rooms.respawn);
    CHECK(s.board.cells[U(r.cell)].kind == CellKind::Room);
    CHECK((r.cell / s.board.width > s.board.height / 2 ? 0 : 1) == half);
    CHECK(std::ranges::find(s.board.weakDens, r.cell) == s.board.weakDens.end());
    CHECK(std::ranges::find(s.board.strongDens, r.cell) == s.board.strongDens.end());
    // 2. It appears at the start of its round.
    while (s.round < r.round && !s.over)
        REQUIRE(Apply(def, s, {ActionKind::EndTurn}));
    CHECK(std::ranges::any_of(s.floor, [&](const Floor &x) { return x.cell == r.cell && x.treasure; }));
    CHECK(s.roomSpawns.empty());

    // 3. An answered event comes back as a new unresolved event in its half.
    MatchState e = NewMatch(def, 7, {MatchDef::Find(def.characters, "bao"), MatchDef::Find(def.characters, "khang"), MatchDef::Find(def.characters, "mai")});
    int ev = -1;
    for (int c = 0; c < static_cast<int>(e.board.cells.size()) && ev < 0; ++c)
        if (e.board.cells[U(c)].event >= 0)
            ev = c;
    REQUIRE(ev >= 0);
    e.units[U(Current(e))].cell = e.now.origin = static_cast<std::int16_t>(ev);
    REQUIRE(Apply(def, e, {ActionKind::Answer, 0}));
    REQUIRE(e.roomSpawns.size() == 1);
    CHECK(e.roomSpawns[0].kind == 0);
    const int back = e.roomSpawns[0].cell;
    while (e.round < e.roomSpawns[0].round && !e.over)
        REQUIRE(Apply(def, e, {ActionKind::EndTurn}));
    CHECK(e.board.cells[U(back)].event >= 0);
    CHECK_FALSE(e.board.cells[U(back)].resolved);
}
