#pragma once

#include <Match.hpp>

#include <vector>

// The faction match's bots (Bots.md): the same Actions a player sends, chosen by a score.
namespace umbra::match
{
    enum class BotRole : std::uint8_t { Carry, Defend, Guard };

    // A role for each character of the faction, indexed by unit (other units: Carry).
    std::vector<BotRole> Roles(const MatchDef &def, const MatchState &s, int faction);

    // The next action for Current(s). Always legal while the match runs.
    Action ChooseAction(const MatchDef &def, const MatchState &s, BotLevel level);
}
