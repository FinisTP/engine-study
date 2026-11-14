#pragma once

#include <Night.hpp>

#include <nlohmann/json_fwd.hpp>

// Saves and the fingerprint of a night (Night.md "Saves").
namespace umbra::night
{
    // A hash of every value in the state: equal nights have equal fingerprints on every compiler.
    std::uint64_t Fingerprint(const NightState &state);

    // The state, tagged with the data's fingerprint so a save is never loaded with other data.
    nlohmann::json SaveNight(const NightDef &def, const NightState &state);
    bool LoadNightState(const NightDef &def, const nlohmann::json &saved, NightState &out, std::vector<core::Diagnostic> &diagnostics);
}
