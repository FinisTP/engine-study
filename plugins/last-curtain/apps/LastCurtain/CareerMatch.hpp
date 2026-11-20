#pragma once

#include <Career.hpp>
#include <Match.hpp>

// Between the career and a match (Cafe.md "Career"): the player's side starts as the career left
// it, and leaves its progress and takings behind.
namespace umbra::night
{
    // The player's characters (side 0) take the career's level, XP, stats, abilities and upgrades.
    void ApplyCareer(const match::MatchDef &def, match::MatchState &s, const cafe::Career &career);
    // The player's characters written back; money += side 0's score * moneyPerEssence; a new day. Returns the money earned.
    int RecordMatch(const match::MatchDef &def, const match::MatchState &s, cafe::Career &career, int moneyPerEssence);
}
