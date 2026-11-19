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
