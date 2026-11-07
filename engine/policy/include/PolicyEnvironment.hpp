#pragma once

#include <PolicyContract.hpp>

#include <memory>
#include <random>

namespace umbra::policy
{
    struct EnvironmentSettings final
    {
        std::string learner = "bandit_leader";
        std::string opponent = "hero";
        int maxRounds = 30;               // then the episode is truncated (not a loss, not a win)
        float opponentRandomness = 0.15F; // chance the opponent picks a random legal action
        bool randomStart = true;          // varied starting health and potions
        int opponentLevel = 1;            // the hero meets the boss after gaining levels
    };

    struct EnvironmentStep final
    {
        float reward = 0.0F;
        bool done = false;      // someone won
        bool truncated = false; // hit maxRounds
        int winner = -1;        // 1 = learner's team, 0 = opponent's, -2 = nobody
    };

    // One-on-one turn fight for training and evaluating a learned enemy (roadmap P8A.1). It runs
    // the real TurnEncounter and shared rules; the opponent plays the conventional AI (with some
    // randomness). The learner decides on its turns only. Rewards: Policy.md.
    class PolicyEnvironment final
    {
    public:
        PolicyEnvironment(const rpg::Content &content, EnvironmentSettings settings);

        // A fresh fight; the same seed gives the same fight. False if an actor is unknown.
        bool Reset(std::uint32_t seed);
        [[nodiscard]] const Contract &GetContract() const noexcept { return contract_; }
        [[nodiscard]] std::vector<float> Observe() const;
        [[nodiscard]] std::vector<std::uint8_t> Mask() const;
        // The learner plays catalogue slot `action` (an illegal slot is played as "wait" and
        // reported), then the opponent until it's the learner's turn again or the fight ends.
        EnvironmentStep Step(std::size_t action);
        // The slot the conventional AI would choose for the learner now (a baseline), or -1.
        [[nodiscard]] int ConventionalAction() const;
        [[nodiscard]] const turn::TurnEncounter *Encounter() const noexcept { return encounter_.get(); }
        [[nodiscard]] int IllegalActions() const noexcept { return illegal_; }

    private:
        void PlayOpponent();
        [[nodiscard]] float HealthFraction(std::size_t index) const;

        static constexpr std::size_t opponentIndex = 0;
        static constexpr std::size_t learnerIndex = 1;
        const rpg::Content &content_;
        EnvironmentSettings settings_;
        Contract contract_;
        std::mt19937 random_;
        std::vector<rpg::Character> fighters_; // owned here; the encounter points into it
        std::unique_ptr<turn::TurnEncounter> encounter_;
        std::uint64_t nextId_ = 1;
        int illegal_ = 0;
    };
}
