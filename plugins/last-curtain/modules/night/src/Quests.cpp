#include "NightInner.hpp"

#include <algorithm>

// Quests (Night.md "Quests"): open to every living guest, each finished once a night by the first.
namespace umbra::night
{
    using namespace inner;

    namespace
    {
        void Finish(const NightDef &def, NightState &state, int g, int q)
        {
            // The race is won: the power (cooling from now) and the stat; a carried item is handed over.
            const QuestDef &quest = def.quests[U(q)];
            Guest &guest = state.guests[U(g)];
            state.questBy[U(q)] = I8(g + 1);
            guest.power = I8(quest.power), guest.powerCooldown = 0;
            std::int16_t *stat = quest.stat == StatKind::Might ? &guest.might : quest.stat == StatKind::Sanity ? &guest.sanity
                               : quest.stat == StatKind::Speed ? &guest.speed : quest.stat == StatKind::MaxHp ? &guest.maxHp : nullptr;
            if (stat)
                *stat = I16(*stat + quest.amount);
            if (quest.stat == StatKind::MaxHp)
                guest.hp = I16(guest.hp + quest.amount);
            if (auto it = std::find(guest.items.begin(), guest.items.end(), I8(quest.item)); quest.kind == QuestKind::Carry && it != guest.items.end())
                *it = -1;
            Log(state, LogKind::Quest, g, -1, q);
        }
    }

    void inner::Quests(const NightDef &def, NightState &state, int g)
    {
        if (!Living(state, g))
            return;
        Guest &guest = state.guests[U(g)];
        const int room = guest.cell >= 0 ? state.cells[U(guest.cell)].room : -1;
        for (std::size_t q = 0; q < def.quests.size(); ++q)
        {
            const QuestDef &quest = def.quests[q];
            if (state.questBy[q] != 0)
                continue;
            bool done = false;
            switch (quest.kind)
            {
            case QuestKind::Carry: // the item in the bag, in the second room
                done = room == quest.roomB && std::find(guest.items.begin(), guest.items.end(), I8(quest.item)) != guest.items.end();
                break;
            case QuestKind::Visit: // the first room, then the second
                if (room == quest.roomA)
                    guest.quest[q] = 1;
                done = room == quest.roomB && guest.quest[q] >= 1;
                break;
            case QuestKind::Slay:
            case QuestKind::Endure:
                done = guest.quest[q] >= quest.count;
                break;
            }
            if (done)
                Finish(def, state, g, static_cast<int>(q));
        }
    }

    void inner::QuestCount(const NightDef &def, NightState &state, int g, QuestKind kind)
    {
        if (!Living(state, g))
            return;
        for (std::size_t q = 0; q < def.quests.size(); ++q)
            if (def.quests[q].kind == kind && state.questBy[q] == 0)
                state.guests[U(g)].quest[q] = I8(std::min(100, state.guests[U(g)].quest[q] + 1));
        Quests(def, state, g);
    }
}
