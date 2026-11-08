#include <EncounterCoordinator.hpp>

namespace umbra::encounter
{
    using rpg::Rejection;

    std::uint64_t EncounterCoordinator::BeginTurn([[maybe_unused]] rpg::Roster roster)
    {
#if UMBRA_PLUGIN_TURN_COMBAT
        if (state_ != State::Exploration)
            return 0;
        roster_ = roster;
        turn_ = std::make_unique<turn::TurnEncounter>(content_, std::move(roster));
        state_ = State::TurnBased;
        CheckOver();
        return ++id_;
#else
        return 0; // no turn-combat plugin in this project
#endif
    }

    std::uint64_t EncounterCoordinator::BeginRealtime([[maybe_unused]] rpg::Roster roster)
    {
#if UMBRA_PLUGIN_REALTIME_COMBAT
        if (state_ != State::Exploration)
            return 0;
        roster_ = roster;
        realtime_ = std::make_unique<realtime::RealtimeEncounter>(content_, std::move(roster));
        state_ = State::Realtime;
        CheckOver();
        return ++id_;
#else
        return 0; // no real-time-combat plugin in this project
#endif
    }

    Rejection EncounterCoordinator::Submit(std::uint64_t encounter, [[maybe_unused]] const rpg::ActionIntent &intent)
    {
        // 1. Made for this fight? (Decisions from a finished fight must never land in the next one.)
        if (state_ == State::Exploration || encounter != id_)
            return Rejection::Stale;
        if (state_ == State::Resolution)
            return Rejection::EncounterOver;
        // 2. Hand it to the one controller that exists.
        Rejection result = Rejection::Stale;
#if UMBRA_PLUGIN_TURN_COMBAT
        if (turn_)
            result = turn_->Submit(intent);
#endif
#if UMBRA_PLUGIN_REALTIME_COMBAT
        if (realtime_)
            result = realtime_->StartAction(intent);
#endif
        CheckOver();
        return result;
    }

    void EncounterCoordinator::Update([[maybe_unused]] float seconds)
    {
#if UMBRA_PLUGIN_REALTIME_COMBAT
        if (state_ != State::Realtime)
            return;
        realtime_->Update(seconds);
        CheckOver();
#endif
    }

    int EncounterCoordinator::Finish()
    {
        if (state_ == State::Exploration)
            return -1;
        int winner = -1;
#if UMBRA_PLUGIN_TURN_COMBAT
        if (turn_)
            winner = turn_->Winner();
        turn_.reset();
#endif
#if UMBRA_PLUGIN_REALTIME_COMBAT
        if (realtime_)
            winner = realtime_->Winner();
        realtime_.reset();
#endif
        for (const rpg::Combatant &c : roster_)
            rpg::EndEncounter(*c.character);
        roster_.clear();
        state_ = State::Exploration;
        return winner;
    }

    void EncounterCoordinator::CheckOver()
    {
#if UMBRA_PLUGIN_TURN_COMBAT
        if (turn_ && turn_->IsOver())
            state_ = State::Resolution;
#endif
#if UMBRA_PLUGIN_REALTIME_COMBAT
        if (realtime_ && realtime_->IsOver())
            state_ = State::Resolution;
#endif
    }
}
