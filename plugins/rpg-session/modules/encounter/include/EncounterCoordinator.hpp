#pragma once

#include <Rules.hpp>
// Each combat mode is a plugin; a project may have one or both (UMBRA_PLUGIN_* from the build).
#if UMBRA_PLUGIN_REALTIME_COMBAT
#include <RealtimeEncounter.hpp>
#endif
#if UMBRA_PLUGIN_TURN_COMBAT
#include <TurnEncounter.hpp>
#endif

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

    // Composes the two independent controllers for a game that uses both (the reference RPG), or
    // the one the project has: Begin of a missing mode returns 0.
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
#if UMBRA_PLUGIN_TURN_COMBAT
        [[nodiscard]] turn::TurnEncounter *Turn() noexcept { return turn_.get(); }
#endif
#if UMBRA_PLUGIN_REALTIME_COMBAT
        [[nodiscard]] realtime::RealtimeEncounter *Realtime() noexcept { return realtime_.get(); }
#endif

    private:
        void CheckOver();

        const rpg::Content &content_;
        State state_ = State::Exploration;
        std::uint64_t id_ = 0;
        rpg::Roster roster_;
#if UMBRA_PLUGIN_TURN_COMBAT
        std::unique_ptr<turn::TurnEncounter> turn_;
#endif
#if UMBRA_PLUGIN_REALTIME_COMBAT
        std::unique_ptr<realtime::RealtimeEncounter> realtime_;
#endif
    };
}
