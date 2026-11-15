#include <NightSave.hpp>

#include "NightInner.hpp"

#include <nlohmann/json.hpp>

// A bitset is saved as its string of 0s and 1s.
namespace nlohmann
{
    template <std::size_t N> struct adl_serializer<std::bitset<N>>
    {
        static void to_json(json &j, const std::bitset<N> &b) { j = b.to_string(); }
        static void from_json(const json &j, std::bitset<N> &b)
        {
            // The last character is bit 0, as to_string writes it; anything but '1' reads as 0.
            const std::string text = j.get<std::string>();
            b.reset();
            for (std::size_t i = 0; i < text.size() && i < N; ++i)
                b[i] = text[text.size() - 1 - i] == '1';
        }
    };
}

namespace umbra::night
{
    // Field lists: every member of every struct in the state.
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Cell, room, doors, opened, resolved, event)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Guest, hp, maxHp, might, sanity, speed, cell, from, level, group, xp, items, bonusUsed, down, dead,
                                       downedBy, pending, asked, chatRound, movesLeft, origin, budget, steps, goal, goalCount,
                                       goalRoomSeen, knows)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FloorItem, cell, item)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LogEntry, tick, kind, who, other, a, b)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Monster, kind, cell, leavesRound, hp, might, sanity)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Fighter, guest, monster, taken, side, buff, buffRounds)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Battle, active, round, turn, count, fighters, cell, owner)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Taken, present, guest, cooldown, reflect)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Mark, cell, direction, untilRound)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChatState, active, asker, answerer, question)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NightState, seed, random, cells, deck, deckSize, deckNext, placed, foyer, understage, guests, order, count,
                                       round, turn, player, over, floor, eventsUsed, itemsUsed, log, logCount, bonds, approvals, marked,
                                       outcome, taken, flooded, sealed, chat, battle, monsters)

    namespace
    {
        std::uint64_t Fnv(const std::string &text)
        {
            std::uint64_t h = 14695981039346656037ULL;
            for (unsigned char c : text)
                h = (h ^ c) * 1099511628211ULL;
            return h;
        }

        // Indexes that would read past the data or the arrays.
        std::string Damaged(const NightDef &def, const NightState &s)
        {
            auto in = [](int v, int lo, int hi) { return v >= lo && v < hi; };
            const int rooms = static_cast<int>(def.rooms.size()), items = static_cast<int>(def.items.size());
            if (s.count != static_cast<int>(def.guests.size()) || !in(s.turn, 0, s.count) || !in(s.foyer, 0, maxCells) || !in(s.deckSize, 0, maxDeck + 1) ||
                !in(s.deckNext, 0, s.deckSize + 1) || !in(s.marked, -1, s.count) || !in(s.player, -1, s.count))
                return "the night's counters are out of range";
            for (const Cell &c : s.cells)
                if (!in(c.room, -1, rooms) || (c.opened && c.room < 0) || !in(c.event, -1, static_cast<int>(def.events.size())))
                    return "a cell names a room that does not exist";
            for (int i = 0; i < s.deckSize; ++i)
                if (!in(s.deck[inner::U(i)], 0, rooms))
                    return "the deck names a room that does not exist";
            for (int g = 0; g < s.count; ++g)
            {
                const Guest &guest = s.guests[inner::U(g)];
                if (!in(guest.cell, 0, maxCells) || !s.cells[inner::U(guest.cell)].opened || !in(s.order[inner::U(g)], 0, s.count) ||
                    !in(guest.downedBy, -1, s.count) || !in(guest.goal, -1, static_cast<int>(def.goals.size())) || !in(guest.origin, -1, maxCells) ||
                    !in(guest.group, -1, s.count + 1) || guest.goalCount < 0)
                    return "guest " + std::to_string(g) + " is out of range (cell " + std::to_string(guest.cell) + ", downedBy " +
                           std::to_string(guest.downedBy) + ")";
                for (std::int8_t i : guest.items)
                    if (!in(i, -1, items))
                        return "a guest carries an item that does not exist";
            }
            for (const FloorItem &f : s.floor)
                if (!in(f.cell, 0, maxCells) || !in(f.item, 0, items))
                    return "a floor item is out of range";
            for (const Monster &m : s.monsters)
                if (!in(m.kind, 0, static_cast<int>(def.monsters.size())) || !in(m.cell, 0, maxCells))
                    return "a monster is out of range";
            const Battle &b = s.battle;
            if (b.active && (!in(b.count, 1, 9) || !in(b.turn, 0, b.count)))
                return "the battle is out of range";
            for (int f = 0; f < b.count && b.active; ++f)
            {
                const Fighter &fi = b.fighters[inner::U(f)];
                if (!in(fi.guest, -1, s.count) || !in(fi.monster, -1, static_cast<int>(s.monsters.size())) || (fi.guest < 0) == (fi.monster < 0))
                    return "a fighter is out of range";
                for (std::int8_t buff : fi.buff)
                    if (!in(buff, -1, static_cast<int>(def.buffs.size())))
                        return "a fighter has a buff that does not exist";
            }
            if (s.taken.present && !in(s.taken.guest, 0, s.count))
                return "the Taken is out of range";
            const ChatState &c = s.chat;
            if (c.active && (!in(c.asker, 0, s.count) || !in(c.answerer, 0, s.count) || !in(c.question, 0, static_cast<int>(def.questions.size()))))
                return "the chat is out of range";
            for (std::int8_t a : s.approvals)
                if (!in(a, 0, def.rules.persuade.approvals + 1))
                    return "an approval count is out of range";
            return {};
        }
    }

    std::uint64_t Fingerprint(const NightState &state) { return Fnv(nlohmann::json(state).dump()); }

    nlohmann::json SaveNight(const NightDef &def, const NightState &state)
    {
        return {{"format", "umbra.night"}, {"version", 4}, {"data", def.fingerprint}, {"state", state}};
    }

    bool LoadNightState(const NightDef &def, const nlohmann::json &saved, NightState &out, std::vector<core::Diagnostic> &diagnostics)
    {
        auto fail = [&](const std::string &message) {
            diagnostics.push_back({core::Severity::Error, "night.save", "", "", message});
            return false;
        };
        if (!saved.is_object() || saved.value("format", "") != "umbra.night" || saved.value("version", 0) != 4)
            return fail("not a night save of version 4");
        if (saved.value("data", std::uint64_t{0}) != def.fingerprint)
            return fail("the save was made with other data; start a new night");
        NightState state;
        try
        {
            saved.at("state").get_to(state);
        }
        catch (const nlohmann::json::exception &e)
        {
            return fail(std::string("the save is damaged: ") + e.what());
        }
        if (const std::string damaged = Damaged(def, state); !damaged.empty())
            return fail("the save is damaged: " + damaged);
        out = std::move(state);
        return true;
    }
}
