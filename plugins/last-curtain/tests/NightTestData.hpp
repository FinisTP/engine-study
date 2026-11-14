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
