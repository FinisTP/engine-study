#pragma once

#include <TurnEncounter.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace umbra::policy
{
    // What a learned turn-based enemy sees and may do. Fixed order, versioned: a model trained
    // for one contract is refused by another. Details and the reasons behind each choice: Policy.md.

    inline constexpr const char *observationVersion = "umbra.obs.turn1v1.v1";
    inline constexpr std::size_t observationSize = 12;
    // Names in vector order, for manifests, reports and debugging.
    inline constexpr std::array<const char *, observationSize> observationNames{
        "self.health", "self.energy", "self.guarded", "self.weakened", "self.poisoned", "self.healItems",
        "opponent.health", "opponent.energy", "opponent.guarded", "opponent.weakened", "opponent.poisoned", "round"};

    // One entry of the action catalogue: a skill, an item, or (both empty) waiting.
    struct ActionSlot final
    {
        std::string skill;
        std::string item;
        friend bool operator==(const ActionSlot &, const ActionSlot &) = default;
    };

    struct Contract final
    {
        std::string learner;  // actor id the policy plays
        std::string opponent; // actor id it was trained against
        std::vector<ActionSlot> actions;
        std::string fingerprint; // hash of every definition that changes what an action means
    };

    // The catalogue: wait, then the learner archetype's turn skills (content order), then the
    // usable items of its starting inventory. Empty actions if an actor is unknown.
    [[nodiscard]] Contract MakeContract(const rpg::Content &content, const std::string &learner, const std::string &opponent);

    // Observation of `self` facing `opponent` (roster indices), every value in 0..1.
    [[nodiscard]] std::vector<float> Observe(const rpg::Content &content, const turn::TurnEncounter &encounter, std::size_t self,
                                             std::size_t opponent, int maxRounds);
    // 1 for each catalogue slot that is legal right now (it must be `self`'s turn).
    [[nodiscard]] std::vector<std::uint8_t> Mask(const turn::TurnEncounter &encounter, const Contract &contract, std::size_t self,
                                                 std::size_t opponent);
    // Slot -> intent: enemy skills target the opponent, self skills and items the actor itself.
    [[nodiscard]] rpg::ActionIntent ToIntent(const rpg::Content &content, const Contract &contract, std::size_t slot,
                                             std::size_t self, std::size_t opponent);
    // Which slot an intent is (for the conventional baseline); -1 if it isn't in the catalogue.
    [[nodiscard]] int ToSlot(const Contract &contract, const rpg::ActionIntent &intent);
    // True if the slot's skill deals damage to the opponent (the "aggressive" choices in reports).
    [[nodiscard]] bool IsAttack(const rpg::Content &content, const ActionSlot &slot);
}
