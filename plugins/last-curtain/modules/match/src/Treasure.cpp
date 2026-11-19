#include <Match.hpp>

#include "MatchInner.hpp"

#include <algorithm>

// An item room's treasure: face-down cards, one chosen (Match.md "Treasure").
namespace umbra::match::inner
{
    void DealCards(const MatchDef &def, MatchState &s)
    {
        // 1. min..max cards, each a treasure.csv row drawn by weight.
        int total = 0;
        for (const TreasureDef &t : def.treasure)
            total += t.weight;
        const int count = def.rules.treasure.min + Roll(s, def.rules.treasure.max - def.rules.treasure.min + 1);
        s.cards.clear();
        for (int c = 0; c < count; ++c)
        {
            int r = Roll(s, total), row = 0;
            while (r >= def.treasure[U(row)].weight)
                r -= def.treasure[U(row++)].weight;
            s.cards.push_back(static_cast<std::int8_t>(row));
        }
    }

    void TakeCard(const MatchDef &def, MatchState &s, int me, int card)
    {
        // 1. The card's reward; an item with no room in the bag becomes essence.
        const int row = s.cards[U(card)];
        const TreasureDef &t = def.treasure[U(row)];
        Unit &u = s.units[U(me)];
        s.cards.clear();
        Log(s, LogKind::Treasure, me, row, t.amount);
        const auto space = std::ranges::find(u.items, std::int8_t(-1));
        const int carried = static_cast<int>(std::ranges::count_if(u.items, [](std::int8_t i) { return i >= 0; }));
        switch (t.kind)
        {
        case TreasureKind::Item:
            if (space != u.items.end() && carried < def.rules.carry)
                *space = static_cast<std::int8_t>(t.item);
            else
                u.essence += def.rules.treasure.fullEssence;
            break;
        case TreasureKind::Essence: u.essence += t.amount; break;
        case TreasureKind::Xp: GainXp(def, s, me, t.amount); break;
        case TreasureKind::Heal: u.hp = static_cast<std::int16_t>(std::min<int>(u.maxHp, u.hp + t.amount)); break;
        }
        Acted(def, s);
    }
}
