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
        // 1. Each guest draws one of its own goals, secretly: it knows only its own.
        for (int g = 0; g < state.count; ++g)
        {
            std::vector<int> mine;
            for (std::size_t i = 0; i < def.goals.size(); ++i)
                if (def.goals[i].guest == g)
                    mine.push_back(static_cast<int>(i));
            Guest &guest = state.guests[U(g)];
            guest.goal = mine.empty() ? I8(-1) : I8(mine[U(Below(state, static_cast<int>(mine.size())))]);
            guest.knows = static_cast<std::uint8_t>(1u << g);
        }
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
}
