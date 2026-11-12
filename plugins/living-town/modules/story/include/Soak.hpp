#pragma once

#include "Storyteller.hpp"

#include <map>

// The soak test (S24): the same town and plan lived on many seeds, and what came of it. This is
// how a developer sees whether the plan holds and how much the playthroughs differ.
namespace umbra::town
{
    struct SoakBeat final
    {
        int natural = 0, forced = 0, missed = 0;
        int firstDay = 0, lastDay = 0;       // earliest and latest day it was reached
        std::map<std::string, int> outcomes; // "Pip stole coins from Tobin" -> playthroughs
    };
    struct SoakReport final
    {
        int seeds = 0, days = 0;
        std::vector<SoakBeat> beats;
        int complete = 0;  // playthroughs that reached every beat
        int forbidden = 0; // troubles that broke `never`, a beat's `forbid` or the leave/die rule (must be 0)
        long long troubles = 0;
        std::map<std::string, int> roads; // the outcomes of all beats in order -> playthroughs
        [[nodiscard]] bool Holds() const { return forbidden == 0 && complete == seeds; }
    };

    [[nodiscard]] SoakReport Soak(const TownDef &def, const PlanDef &plan, std::uint64_t firstSeed, int seeds, int days);
    [[nodiscard]] std::string Text(const PlanDef &plan, const SoakReport &report);
    [[nodiscard]] nlohmann::json Json(const PlanDef &plan, const SoakReport &report);
}
