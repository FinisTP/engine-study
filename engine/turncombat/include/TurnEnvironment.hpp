#pragma once

#include <TurnEncounter.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace umbra::turn
{
    // One fighter of a repeatable scenario: which archetype, which instance id, which team.
    struct ScenarioSlot final
    {
        std::string actor;
        std::string id;
        int team = 0;
    };

    struct StepResult final
    {
        rpg::Rejection rejection = rpg::Rejection::None;
        rpg::Events events; // what this step caused, including the next turn's status ticks
        bool done = false;
        int winner = -1;
    };

    // The seam a training tool (P8A) or a scripted test drives: reset to a fresh scenario,
    // list legal actions, step one decision. It wraps the same TurnEncounter and shared rules
    // the game uses; it adds no rules of its own (roadmap T55, T60). No training framework here.
    class TurnEnvironment final
    {
    public:
        TurnEnvironment(const rpg::Content &content, std::vector<ScenarioSlot> scenario);

        // Rebuilds every fighter from its definition and starts a new encounter (round 1,
        // empty decision log, no statuses). False if an archetype is unknown.
        bool Reset();

        // The fighter that must decide now; nothing when the episode is over.
        [[nodiscard]] std::optional<std::size_t> Current() const { return encounter_ ? encounter_->Current() : std::nullopt; }
        [[nodiscard]] std::vector<rpg::ActionIntent> LegalActions() const;
        // Submits one decision for the current fighter. The environment numbers decisions itself.
        StepResult Step(rpg::ActionIntent intent);

        // Read-only view of the fighters (the full state; a policy must only see what its
        // observation contract allows, which is decided in P8A).
        [[nodiscard]] const std::vector<rpg::Character> &Characters() const noexcept { return characters_; }
        [[nodiscard]] int Round() const { return encounter_ ? encounter_->Round() : 0; }
        [[nodiscard]] int Steps() const noexcept { return steps_; }

    private:
        const rpg::Content &content_;
        std::vector<ScenarioSlot> scenario_;
        std::vector<rpg::Character> characters_; // owned here; the encounter points into it
        std::unique_ptr<TurnEncounter> encounter_;
        std::uint64_t nextId_ = 1;
        int steps_ = 0;
    };
}
