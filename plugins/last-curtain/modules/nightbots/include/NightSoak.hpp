#pragma once

#include <Night.hpp>

#include <nlohmann/json_fwd.hpp>

#include <string>

// The soak test: many bot nights, one report to balance by (Balance.md).
namespace umbra::night
{
    struct SoakForm
    {
        int nights = 0, companyWins = 0; // nights: hunts against this form
    };

    struct SoakReport
    {
        int nights = 0, faults = 0;
        std::array<SoakForm, maxGuests> forms{}; // by marked guest
        int companyWins = 0, takenWins = 0, allFinished = 0; // allFinished: everyone did their goal, no hunt
        int playerWins = 0;                        // nights guest 0's side won
        long long rooms = 0, monsterKills = 0, minionKills = 0, levels = 0, downs = 0, deaths = 0;
        std::array<int, maxGuests> marked{};        // nights each guest was the Taken
        std::vector<int> goalsDrawn, goalsDone;      // by goals.csv row
        long long roomsByMidnight = 0;              // summed over the nights
        int eveningRounds = 0, roundsWithBotChat = 0; // a chat answered between two bots

        // No faults, and every band of rules.json "balance" holds (forms and goals: 50 samples or more).
        bool Holds(const NightDef &def) const;
    };

    // strategy: 0 the bots, 1 the company leaves minions alone.
    SoakReport Soak(const NightDef &def, std::uint64_t firstSeed, int nights, int strategy = 0);
    std::string Text(const NightDef &def, const SoakReport &report);
    nlohmann::json Json(const NightDef &def, const SoakReport &report);
}
