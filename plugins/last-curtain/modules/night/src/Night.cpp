#include "NightInner.hpp"

namespace umbra::night
{
    using namespace inner;

    NightState NewNight(const NightDef &def, std::uint64_t seed, int player)
    {
        NightState state;
        state.seed = seed;
        // splitmix64 spreads neighbouring seeds apart (xorshift alone keeps them alike for many draws).
        std::uint64_t z = seed + 0x9E3779B97F4A7C15ULL;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        state.random = (z ^ (z >> 31)) | 1u;
        state.player = I8(player);
        state.count = I8(def.guests.size());
        // 1. The whole map, and a start room for each guest, far apart.
        std::vector<int> starts = BuildBoard(def, state);

        // 2. Every guest in the foyer at full HP; a d20 each sets the order for the night.
        std::array<int, maxGuests> rolls{}, speeds{};
        for (int g = 0; g < state.count; ++g)
        {
            const GuestDef &d = def.guests[U(g)];
            Guest &guest = state.guests[U(g)];
            guest.hp = guest.maxHp = I16(d.hp);
            guest.might = I16(d.might);
            guest.sanity = I16(d.sanity);
            guest.speed = I16(d.speed);
            // Each guest in a start room of its own, by chance; the room gives it nothing on this entry.
            if (g < static_cast<int>(starts.size()))
                std::swap(starts[U(g)], starts[U(g + Below(state, static_cast<int>(starts.size()) - g))]);
            guest.cell = guest.from = I8(g < static_cast<int>(starts.size()) ? starts[U(g)] : state.foyer);
            guest.bonusUsed.set(U(guest.cell));
            rolls[U(g)] = Roll(state, 20);
            speeds[U(g)] = d.speed;
        }
        state.order = Order(rolls, speeds, state.count);
        for (const BondDef &b : def.bonds)
            state.bonds[U(b.from * maxGuests + b.to)] = I16(b.bond);
        StartTurn(def, state);
        return state;
    }

    void inner::Dawn(const NightDef &def, NightState &state)
    {
        // 1. Fewer than two alive: the night has already been decided (Hurt ends it).
        if (LivingCount(state) < 2)
            return EndNight(state, LivingCount(state) == 1 ? Outcome::Escaped : Outcome::House);
        // 2. A new night on a new map: the board, its rooms, monsters, marks and floor start afresh.
        ++state.night;
        state.round = 1;
        state.cells = {};
        state.understage = state.foyer = -1;
        state.floor.clear(), state.monsters.clear();
        state.flooded = {}, state.sealed = {};
        state.eventsUsed = state.itemsUsed = 0;
        state.battle = {}, state.chat = {};
        state.killer = {}; // asleep until midnight again
        state.questBy = {}; // the quests open again
        for (Guest &g : state.guests)
            g.quest = {};
        std::vector<int> starts = BuildBoard(def, state);
        // 3. The living keep what they have and wake healed, far apart; the ghosts wait at the under-stage.
        std::size_t next = 0;
        for (int g = 0; g < state.count; ++g)
        {
            Guest &guest = state.guests[U(g)];
            guest.bonusUsed.reset();
            if (Living(state, g))
            {
                guest.cell = guest.from = I8(next < starts.size() ? starts[next++] : state.foyer);
                guest.bonusUsed.set(U(guest.cell));
                guest.hp = I16(Stat(def, state, g, StatKind::MaxHp));
            }
            else if (guest.cell >= 0 || guest.returnRound <= 1)
                guest.cell = state.understage, guest.hp = I16(def.rules.ghost.hp), guest.returnRound = 0;
        }
        Log(state, LogKind::Dawn, -1, -1, state.night);
    }
}
