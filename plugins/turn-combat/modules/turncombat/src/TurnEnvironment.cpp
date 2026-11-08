#include <TurnEnvironment.hpp>

namespace umbra::turn
{
    TurnEnvironment::TurnEnvironment(const rpg::Content &content, std::vector<ScenarioSlot> scenario)
        : content_(content), scenario_(std::move(scenario))
    {
    }

    bool TurnEnvironment::Reset()
    {
        // 1. Drop the old encounter first: it points into the old characters.
        encounter_.reset();
        characters_.clear();
        nextId_ = 1;
        steps_ = 0;

        // 2. Fresh characters straight from the definitions; nothing survives from the last episode.
        for (const ScenarioSlot &slot : scenario_)
        {
            std::optional<rpg::Character> character = rpg::MakeCharacter(content_, slot.actor, slot.id);
            if (!character)
                return false;
            characters_.push_back(std::move(*character));
        }
        rpg::Roster roster;
        for (std::size_t i = 0; i < characters_.size(); ++i)
            roster.push_back({&characters_[i], scenario_[i].team});
        encounter_ = std::make_unique<TurnEncounter>(content_, std::move(roster));
        return true;
    }

    std::vector<rpg::ActionIntent> TurnEnvironment::LegalActions() const
    {
        return encounter_ ? encounter_->LegalActions() : std::vector<rpg::ActionIntent>{};
    }

    StepResult TurnEnvironment::Step(rpg::ActionIntent intent)
    {
        StepResult result;
        if (!encounter_)
        {
            result.rejection = rpg::Rejection::EncounterOver;
            return result;
        }
        intent.id = nextId_++;
        result.rejection = encounter_->Submit(intent);
        if (result.rejection == rpg::Rejection::None)
            ++steps_;
        result.events = encounter_->TakeEvents();
        result.done = encounter_->IsOver();
        result.winner = encounter_->Winner();
        return result;
    }
}
