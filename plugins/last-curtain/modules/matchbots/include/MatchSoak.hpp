#pragma once

#include <MatchBots.hpp>

#include <array>
#include <string>

// The soak: bots against bots on many seeds, and the balance bands (Bots.md "The soak").
namespace umbra::match
{
    struct MatchReport
    {
        int runs = 0, faults = 0, draws = 0; // faults: a match that did not end
        std::array<int, 2> wins{};
        int firstMoverWins = 0;          // matches won by the side that moves first
        int leads = 0, comebacks = 0;    // a side ahead after two thirds of the rounds; matches the other won
        int guardianFalls = 0;
        long long fromMonsters = 0, fromKills = 0, fromGuardians = 0; // essence gained, by source
        long long performed = 0;         // essence turned into score
        double decidedRound = 0;         // mean last round the lead changed hands

        bool Holds(const MatchDef &def) const;
    };

    MatchReport SoakMatches(const MatchDef &def, std::uint64_t firstSeed, int runs, BotLevel side0, BotLevel side1);
    std::string Text(const MatchDef &def, const MatchReport &r);
}
