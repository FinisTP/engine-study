#include <PolicyEnvironment.hpp>

#include <Character.hpp>
#include <Inventory.hpp>
#include <Progression.hpp>

namespace umbra::policy
{
    PolicyEnvironment::PolicyEnvironment(const rpg::Content &content, EnvironmentSettings settings)
        : content_(content), settings_(std::move(settings)), contract_(MakeContract(content, settings_.learner, settings_.opponent))
    {
    }

    bool PolicyEnvironment::Reset(std::uint32_t seed)
    {
        // 1. Fresh characters from their definitions.
        random_.seed(seed);
        std::optional<rpg::Character> opponent = rpg::MakeCharacter(content_, settings_.opponent, settings_.opponent);
        std::optional<rpg::Character> learner = rpg::MakeCharacter(content_, settings_.learner, settings_.learner);
        if (!opponent || !learner || contract_.actions.empty())
            return false;

        // 2. The opponent's level (level-ups restore health), then varied starts, so the learner
        //    also meets weak and strong opponents.
        if (settings_.opponentLevel > 1)
            rpg::GrantXp(content_, *opponent, rpg::XpForLevel(settings_.opponentLevel));
        if (settings_.randomStart)
        {
            std::uniform_real_distribution<float> unit(0.0F, 1.0F);
            const auto scale = [&](rpg::Character &c, float low) {
                const int max = rpg::Get(rpg::DerivedStats(content_, c), rpg::Stat::MaxHealth);
                c.health = std::max(1, static_cast<int>(static_cast<float>(max) * (low + (1.0F - low) * unit(random_))));
            };
            scale(*opponent, 0.25F);
            scale(*learner, 0.4F);
            const int potions = rpg::CountItem(*opponent, "health_potion");
            rpg::RemoveItem(*opponent, "health_potion", potions - std::uniform_int_distribution<int>(0, potions)(random_));
        }

        // 3. Opponent is team 0 at index 0, learner team 1 at index 1; play until it's the learner's turn.
        fighters_ = {std::move(*opponent), std::move(*learner)};
        encounter_ = std::make_unique<turn::TurnEncounter>(content_, rpg::Roster{{&fighters_[0], 0}, {&fighters_[1], 1}});
        nextId_ = 1;
        illegal_ = 0;
        PlayOpponent();
        return true;
    }

    std::vector<float> PolicyEnvironment::Observe() const
    {
        return policy::Observe(content_, *encounter_, learnerIndex, opponentIndex, settings_.maxRounds);
    }

    std::vector<std::uint8_t> PolicyEnvironment::Mask() const { return policy::Mask(*encounter_, contract_, learnerIndex, opponentIndex); }

    float PolicyEnvironment::HealthFraction(std::size_t index) const
    {
        const rpg::Character &c = fighters_[index];
        return static_cast<float>(c.health) / static_cast<float>(rpg::Get(rpg::DerivedStats(content_, c), rpg::Stat::MaxHealth));
    }

    void PolicyEnvironment::PlayOpponent()
    {
        // The opponent acts on its turns: the conventional AI, or now and then a random legal move.
        std::uniform_real_distribution<float> unit(0.0F, 1.0F);
        while (!encounter_->IsOver() && encounter_->Current() == opponentIndex)
        {
            rpg::ActionIntent intent = turn::ChooseTurnAction(content_, *encounter_);
            if (unit(random_) < settings_.opponentRandomness)
            {
                const std::vector<rpg::ActionIntent> legal = encounter_->LegalActions();
                if (!legal.empty())
                    intent = legal[std::uniform_int_distribution<std::size_t>(0, legal.size() - 1)(random_)];
            }
            intent.id = nextId_++;
            encounter_->Submit(intent);
            (void)encounter_->TakeEvents();
        }
    }

    EnvironmentStep PolicyEnvironment::Step(std::size_t action)
    {
        EnvironmentStep step;
        if (!encounter_ || encounter_->IsOver())
        {
            step.done = true;
            return step;
        }
        // 1. The learner's move (an illegal slot is counted and played as waiting).
        const float dealtBefore = HealthFraction(opponentIndex), takenBefore = HealthFraction(learnerIndex);
        if (action >= contract_.actions.size() || Mask()[action] == 0)
        {
            ++illegal_;
            action = 0;
        }
        rpg::ActionIntent intent = ToIntent(content_, contract_, action, learnerIndex, opponentIndex);
        intent.id = nextId_++;
        encounter_->Submit(intent);
        (void)encounter_->TakeEvents();

        // 2. The opponent's reply, then the reward: shaped by health swings, decided by the outcome.
        PlayOpponent();
        step.reward = 0.5F * ((dealtBefore - HealthFraction(opponentIndex)) - (takenBefore - HealthFraction(learnerIndex)));
        if (encounter_->IsOver())
        {
            step.done = true;
            step.winner = encounter_->Winner();
            step.reward += step.winner == 1 ? 1.0F : (step.winner == 0 ? -1.0F : 0.0F);
        }
        else if (encounter_->Round() > settings_.maxRounds)
            step.truncated = true;
        return step;
    }

    int PolicyEnvironment::ConventionalAction() const
    {
        if (!encounter_ || encounter_->Current() != learnerIndex)
            return -1;
        return ToSlot(contract_, turn::ChooseTurnAction(content_, *encounter_));
    }
}
