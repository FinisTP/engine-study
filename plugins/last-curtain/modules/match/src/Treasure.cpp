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

    void RoomBack(const MatchDef &def, MatchState &s, int claimed, int kind)
    {
        // 1. A free room in the claimed one's half: a room square, no den, no event or treasure, none already coming.
        const Board &b = s.board;
        const bool bottom = claimed / b.width > b.height / 2;
        std::vector<int> free;
        for (int c = 0; c < static_cast<int>(b.cells.size()); ++c)
        {
            const Cell &cell = b.cells[U(c)];
            const bool taken = std::ranges::find(b.weakDens, c) != b.weakDens.end() || std::ranges::find(b.strongDens, c) != b.strongDens.end() ||
                               (cell.event >= 0 && !cell.resolved) || std::ranges::any_of(s.floor, [&](const Floor &f) { return f.cell == c && f.treasure; }) ||
                               std::ranges::any_of(s.roomSpawns, [&](const RoomSpawn &r) { return r.cell == c; });
            if (cell.kind == CellKind::Room && (c / b.width > b.height / 2) == bottom && !taken && c != claimed)
                free.push_back(c);
        }
        if (free.empty())
            return;
        s.roomSpawns.push_back({static_cast<std::int16_t>(free[U(Roll(s, static_cast<int>(free.size())))]), static_cast<std::int8_t>(kind), s.round + def.rules.rooms.respawn});
    }

    void RunRoomSpawns(const MatchDef &def, MatchState &s)
    {
        // 1. Due rooms open: an event drawn anew, or a treasure on the floor.
        for (std::size_t i = 0; i < s.roomSpawns.size();)
        {
            const RoomSpawn r = s.roomSpawns[i];
            if (r.round > s.round)
            {
                ++i;
                continue;
            }
            if (r.kind == 0 && !def.events.empty())
                s.board.cells[U(r.cell)].event = static_cast<std::int8_t>(Roll(s, static_cast<int>(def.events.size()))), s.board.cells[U(r.cell)].resolved = false;
            else if (r.kind == 1)
                s.floor.push_back({r.cell, -1, 0, true});
            s.roomSpawns.erase(s.roomSpawns.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
}
