#pragma once

#include <Night.hpp>

#include <catch2/catch_test_macros.hpp>

// The shipped data, loaded once per call.
inline umbra::night::NightDef Data()
{
    std::vector<umbra::core::Diagnostic> diagnostics;
    umbra::night::NightDef def;
    REQUIRE(umbra::night::LoadNight(UMBRA_NIGHT_DATA, def, diagnostics));
    return def;
}

// A room with no doors: opened (guests may stand there) but reached by nobody, lit by nobody.
inline void Closet(const umbra::night::NightDef &def, umbra::night::NightState &state, int cell)
{
    const int room = umbra::night::NightDef::Find(def.rooms, "auditorium");
    state.cells[static_cast<std::size_t>(cell)] = {static_cast<std::int8_t>(room), 0, true};
}

// The whole map of a new night with every guest in the foyer (the battle royale lays the map at once).
// For tests of rules that do not depend on the start; the spread-out start is tested in RoyaleTests (T229).
inline umbra::night::NightState FoyerNight(const umbra::night::NightDef &def, std::uint64_t seed, int player = -1)
{
    using namespace umbra::night;
    NightState state = NewNight(def, seed, player);
    state.floor.clear(); // no quest items lying about (RoyaleTests covers them)
    for (int g = 0; g < state.count; ++g)
    {
        Guest &guest = state.guests[static_cast<std::size_t>(g)];
        guest.cell = guest.from = guest.origin = state.foyer;
        guest.bonusUsed.reset();
    }
    return state;
}

// Only the foyer opened, every guest in it: for tests that build a board by opening rooms (Open).
inline umbra::night::NightState BareFoyer(const umbra::night::NightDef &def, std::uint64_t seed, int player = -1)
{
    using namespace umbra::night;
    NightState state = FoyerNight(def, seed, player);
    for (int c = 0; c < maxCells; ++c)
        if (c != state.foyer)
            state.cells[static_cast<std::size_t>(c)] = {};
    state.placed = 1, state.deckNext = 0, state.understage = -1;
    return state;
}

// Opens `n` rooms in a row from `from`; fewer if a room has no door onward.
inline std::vector<int> Chain(const umbra::night::NightDef &def, umbra::night::NightState &state, int from, int n)
{
    std::vector<int> cells;
    for (int at = from; static_cast<int>(cells.size()) < n;)
    {
        int next = -1;
        for (int d = 0; d < 4 && next < 0; ++d)
            if (umbra::night::Doorway(def, state, at, d))
                next = umbra::night::Open(def, state, at, d);
        if (next < 0)
            break;
        cells.push_back(at = next);
    }
    return cells;
}
