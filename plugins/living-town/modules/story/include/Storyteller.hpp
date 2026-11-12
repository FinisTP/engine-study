#pragma once

#include "Plan.hpp"

#include <nlohmann/json_fwd.hpp>

// The storyteller (S24): once a day it decides which trouble, if any, happens in the town, so
// that the plan's beats arrive inside their windows by whatever road this town took.
// How it filters, nudges, looks ahead and forces: Storyteller.md.
namespace umbra::town
{
    enum class BeatHow : std::uint8_t { Waiting, Natural, Forced, Missed };
    struct BeatState final
    {
        BeatHow how = BeatHow::Waiting;
        std::int32_t tick = 0;
        Trouble outcome; // what reached it
    };

    struct StoryState final
    {
        std::vector<BeatState> beats;
        std::int32_t lastTroubleDay = -100;
        std::int32_t troubles = 0; // how many the storyteller has let happen
        TroubleKind lastKind = TroubleKind::Ill;
        std::vector<std::string> warnings; // forced and missed beats, for the developer (newest last)
        [[nodiscard]] int Next() const; // the first beat still waiting (-1 = the plot is done)
    };

    [[nodiscard]] StoryState NewStory(const PlanDef &plan);
    // The filter: may this trouble happen now under the plan? (Possible, not forbidden, within
    // the pacing budget, not too early for its beat, and leaving or dying only where allowed.)
    [[nodiscard]] bool Permitted(const TownDef &def, const PlanDef &plan, const TownState &state, const StoryState &story, const Trouble &trouble);
    // Call after every Tick: acts once a day, at the plan's hour.
    void StoryStep(const TownDef &def, const PlanDef &plan, TownState &state, StoryState &story);
    // Tick + StoryStep, `ticks` times.
    void RunStory(const TownDef &def, const PlanDef &plan, TownState &state, StoryState &story, int ticks);

    [[nodiscard]] nlohmann::json SaveStory(const StoryState &story);
    bool LoadStory(const PlanDef &plan, const nlohmann::json &saved, StoryState &out);
}
