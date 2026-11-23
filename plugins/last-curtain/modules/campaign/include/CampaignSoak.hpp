#pragma once

#include <Campaign.hpp>

#include <map>
#include <string>
#include <vector>

// The campaign soak (Campaign.md "The soak"): bots play a stage many times, both sides.
namespace umbra::campaign
{
    struct StageReport
    {
        int runs = 0, wins = 0, trapped = 0, otherLosses = 0, draws = 0;
        double rounds = 0.0, minutes = 0.0; // averages per run
        double playerFalls = 0.0;           // share of runs where the player fell at least once
        std::map<std::string, int> falls;   // party falls by cast id, over all runs
        std::map<std::string, int> found;   // items taken or dropped into `found`, over all runs
    };

    // runs fights from seed on; the party is the player, Bao, Ba Mai up to the cap, at the stage's level.
    StageReport SoakStage(const match::MatchDef &def, const Cast &cast, const Growth &growth, const StageDef &stage, int refill, std::uint64_t seed, int runs);
    // The bands the report misses ({"wins": [lo, hi], "trapped": ..., "playerFalls": ..., "minutes": ...}), each as a line.
    std::vector<std::string> Missed(const StageReport &r, const nlohmann::json &bands);
    std::string Text(const StageDef &stage, const StageReport &r);
}
