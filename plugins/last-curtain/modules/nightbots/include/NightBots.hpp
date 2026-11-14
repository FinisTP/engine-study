#pragma once

#include <Night.hpp>

// The bots: rules, not the model (Balance.md "The bots"). A bot reads only what its guest could know.
namespace umbra::night
{
    // The action the bot of `guest` takes now (guest must be Current()).
    // chaseMinions false: the company leaves the Taken's minions alone (a soak strategy).
    Action ChooseAction(const NightDef &def, const NightState &state, int guest, bool chaseMinions = true);

    // Every guest by its bot until the night ends; at most 5,000 Applies. Returns how many were made.
    int PlayNight(const NightDef &def, NightState &state);
}
