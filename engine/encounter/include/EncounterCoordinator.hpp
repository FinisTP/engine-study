#pragma once

#include <RealtimeEncounter.hpp>
#include <TurnEncounter.hpp>

#include <cstdint>
#include <memory>

namespace umbra::encounter
{
    enum class State
    {
        Exploration, // no fight
        TurnBased,
        Realtime,
        Resolution // the fight has a result; Finish() returns to exploration
    };

    // Composes the two independent controllers for a game that uses both (the reference RPG).
    // At most one controller exists at a time, modes only change between encounters, and
    // every decision names the encounter it was made for. Rules in Encounter.md.
    class EncounterCoordinator final
    {
    public:
        explicit EncounterCoordinator(const rpg::Content &content) : content_(content) {}

        // Start a fight from exploration. Returns the new encounter's id, or 0 if a fight is
        // already running or about to be finished.
        std::uint64_t BeginTurn(rpg::Roster roster);
        std::uint64_t BeginRealtime(rpg::Roster roster);

        // Decisions for the current encounter. A decision made for an earlier encounter is
        // Stale; once the fight is decided everything is EncounterOver.
        rpg::Rejection Submit(std::uint64_t encounter, const rpg::ActionIntent &intent);
        // Real-time only: advances time (does nothing in other states).
        void Update(float seconds);

        // Ends the fight (decided, or abandoned e.g. by fleeing): statuses end for every fighter
        // (user decision 2026-11-07), the controller and its cooldowns are destroyed.
        // Returns the winning team (-1 if abandoned undecided).
        int Finish();

        [[nodiscard]] State GetState() const noexcept { return state_; }
        [[nodiscard]] std::uint64_t CurrentEncounter() const noexcept { return state_ == State::Exploration ? 0 : id_; }
        // The running controller, for AI, placement and events; nullptr in other modes.
        [[nodiscard]] turn::TurnEncounter *Turn() noexcept { return turn_.get(); }
        [[nodiscard]] realtime::RealtimeEncounter *Realtime() noexcept { return realtime_.get(); }

    private:
        void CheckOver();

        const rpg::Content &content_;
        State state_ = State::Exploration;
        std::uint64_t id_ = 0;
        rpg::Roster roster_;
        std::unique_ptr<turn::TurnEncounter> turn_;
        std::unique_ptr<realtime::RealtimeEncounter> realtime_;
    };
}
