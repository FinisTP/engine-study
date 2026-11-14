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
        BuildBoard(def, state);

        // Every guest in the foyer at full HP; a d20 each sets the order for the night.
        state.count = I8(def.guests.size());
        std::array<int, maxGuests> rolls{}, speeds{};
        for (int g = 0; g < state.count; ++g)
        {
            const GuestDef &d = def.guests[U(g)];
            Guest &guest = state.guests[U(g)];
            guest.hp = guest.maxHp = I16(d.hp);
            guest.might = I16(d.might);
            guest.sanity = I16(d.sanity);
            guest.speed = I16(d.speed);
            guest.cell = state.foyer;
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
