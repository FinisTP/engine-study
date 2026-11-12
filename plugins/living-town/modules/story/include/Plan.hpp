#pragma once

#include <Town.hpp>

#include <filesystem>
#include <string>
#include <vector>

// The outcome plan (S24, ADR 0011): what the developer says the story must, may and must never
// do (`town/*.plan.json`). The storyteller (Storyteller.hpp) keeps a town to it. Storyteller.md.
namespace umbra::town
{
    inline constexpr int anyone = -1;
    inline constexpr int noLastDay = 1 << 20;

    // A kind of trouble, with or without names: {theft, anyone, tobin} = "someone steals from Tobin".
    struct Pattern final
    {
        TroubleKind kind = TroubleKind::Ill;
        int who = anyone, other = anyone;
        int from = 1, to = noLastDay; // days it holds for (only read for `allow`)
        [[nodiscard]] bool Matches(const Trouble &trouble) const;
    };

    // One step of the main plot: between day `from` and day `to`, one of `outcomes` must happen.
    struct BeatDef final
    {
        std::string id, name;
        int from = 1, to = 1;
        std::vector<Pattern> outcomes; // any one of these reaches the beat (the allowed variation)
        std::vector<Pattern> forbid;   // must not happen while this beat is the next one
    };

    struct PlanDef final
    {
        std::vector<BeatDef> beats;  // in story order: a beat opens when the one before is reached
        std::vector<Pattern> never;  // must not happen in any playthrough
        std::vector<Pattern> allow;  // who may leave or die, and in which days (else nobody does)
        // Pacing: how much trouble at once.
        int quietDays = 1; // days of peace after any trouble
        int maxIll = 2;    // people ill at the same time
        int maxGone = 1;   // people the town may lose in one playthrough
        int lookAhead = 2; // days the storyteller rehearses ahead when it nudges
        int hour = 12;     // the hour of the day it acts
    };

    // Reads and checks a plan against its town; false with diagnostics (code "plan.*").
    bool ParsePlan(const std::string &text, const std::string &file, const TownDef &def, PlanDef &out, std::vector<core::Diagnostic> &diagnostics);
    bool LoadPlan(const std::filesystem::path &file, const TownDef &def, PlanDef &out, std::vector<core::Diagnostic> &diagnostics);
    [[nodiscard]] const char *KindName(TroubleKind kind); // "ill", "theft", "feud", "leave", "death"
}
