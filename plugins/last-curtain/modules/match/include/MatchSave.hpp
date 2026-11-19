#pragma once

#include <Match.hpp>

#include <nlohmann/json.hpp>

#include <string>

// A faction match saved as JSON (Match.md "Saves").
namespace umbra::match
{
    nlohmann::json SaveMatch(const MatchState &s);
    // Refuses another format or version, or a malformed file, with a reason; `out` is untouched then.
    bool LoadMatch(const nlohmann::json &j, MatchState &out, std::string &error);
    std::uint64_t Fingerprint(const MatchState &s); // every field but the log
}
