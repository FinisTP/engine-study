#pragma once

#include <Night.hpp>

#include <nlohmann/json_fwd.hpp>

#include <string>

// The soak test: many bot runs (nights until one guest is left), one report to balance by (Balance.md).
namespace umbra::night
{
    struct SoakReport
    {
        int runs = 0, faults = 0;                 // faults: a run that did not end
        int escapes = 0, houseWins = 0;
        std::array<int, maxGuests> wins{};        // runs each guest escaped
        long long nights = 0, deaths = 0;         // nights played, living guests who died
        long long byGuests = 0, byGhosts = 0, byHouse = 0; // who dealt the deaths
        long long monsterKills = 0, levels = 0, quests = 0, powers = 0; // quests finished, powers used

        // No faults, and every band of rules.json "balance" holds.
        bool Holds(const NightDef &def) const;
    };

    SoakReport Soak(const NightDef &def, std::uint64_t firstSeed, int runs, int strategy = 0);
    std::string Text(const NightDef &def, const SoakReport &report);
    nlohmann::json Json(const NightDef &def, const SoakReport &report);
}
