#include <EncounterCoordinator.hpp>

namespace umbra::encounter
{
    using rpg::Rejection;

    std::uint64_t EncounterCoordinator::BeginTurn(rpg::Roster roster)
    {
        if (state_ != State::Exploration)
            return 0;
        roster_ = roster;
        turn_ = std::make_unique<turn::TurnEncounter>(content_, std::move(roster));
        state_ = State::TurnBased;
        CheckOver();
        return ++id_;
    }

    std::uint64_t EncounterCoordinator::BeginRealtime(rpg::Roster roster)
    {
        if (state_ != State::Exploration)
            return 0;
        roster_ = roster;
        realtime_ = std::make_unique<realtime::RealtimeEncounter>(content_, std::move(roster));
        state_ = State::Realtime;
        CheckOver();
        return ++id_;
    }

    Rejection EncounterCoordinator::Submit(std::uint64_t encounter, const rpg::ActionIntent &intent)
    {
        // 1. Made for this fight? (Decisions from a finished fight must never land in the next one.)
        if (state_ == State::Exploration || encounter != id_)
            return Rejection::Stale;
        if (state_ == State::Resolution)
            return Rejection::EncounterOver;
        // 2. Hand it to the one controller that exists.
        const Rejection result = turn_ ? turn_->Submit(intent) : realtime_->StartAction(intent);
        CheckOver();
        return result;
    }

    void EncounterCoordinator::Update(float seconds)
    {
        if (state_ != State::Realtime)
            return;
        realtime_->Update(seconds);
        CheckOver();
    }

    int EncounterCoordinator::Finish()
    {
        if (state_ == State::Exploration)
            return -1;
        const int winner = turn_ ? turn_->Winner() : realtime_->Winner();
        for (const rpg::Combatant &c : roster_)
            rpg::EndEncounter(*c.character);
        turn_.reset();
        realtime_.reset();
        roster_.clear();
        state_ = State::Exploration;
        return winner;
    }

    void EncounterCoordinator::CheckOver()
    {
        if ((turn_ && turn_->IsOver()) || (realtime_ && realtime_->IsOver()))
            state_ = State::Resolution;
    }
}
