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
