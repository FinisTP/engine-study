#pragma once

#include <Rules.hpp>

#include <glm/vec2.hpp>

#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace umbra::realtime
{
    // An action goes Windup -> (commit: cost paid) -> Active (hits) -> Recovery -> Idle.
    enum class Phase
    {
        Idle,
        Windup,
        Active,
        Recovery
    };

    // Real-time combat: time in seconds, targets found by range and facing when the attack is
    // active. Knows nothing about turns. Movement belongs to gameplay/physics: the caller
    // reports where everyone stands each frame. Rules are in RealtimeCombat.md.
    class RealtimeEncounter final
    {
    public:
        // Timing for using an item (items have no timing of their own).
        static constexpr rpg::RealtimeTiming itemTiming{0.5F, 0.05F, 0.3F, 0.0F, 0.0F, 360.0F};

        RealtimeEncounter(const rpg::Content &content, rpg::Roster roster);

        // Ground position (x, z) and facing yaw (0 faces -z, like the player controller).
        void SetPlacement(std::size_t index, glm::vec2 position, float yaw);
        [[nodiscard]] glm::vec2 GetPosition(std::size_t index) const { return fighters_[index].position; }
        [[nodiscard]] float GetYaw(std::size_t index) const { return fighters_[index].yaw; }

        // Starts the wind-up. Nothing is paid yet; anything rejected changes nothing.
        rpg::Rejection StartAction(const rpg::ActionIntent &intent);
        // Advances phases, hits, cooldowns and statuses by `seconds`.
        void Update(float seconds);

        [[nodiscard]] Phase GetPhase(std::size_t index) const { return fighters_[index].phase; }
        [[nodiscard]] float CooldownLeft(std::size_t index, const std::string &skill) const;
        [[nodiscard]] bool IsOver() const noexcept { return winner_ != -1; }
        [[nodiscard]] int Winner() const noexcept { return winner_; }
        [[nodiscard]] const rpg::Roster &GetRoster() const noexcept { return roster_; }
        rpg::Events TakeEvents() { return std::exchange(events_, {}); }

    private:
        struct Fighter final
        {
            glm::vec2 position{0.0F};
            float yaw = 0.0F;
            Phase phase = Phase::Idle;
            float phaseLeft = 0.0F;           // seconds left in the current phase
            rpg::ActionIntent action;         // what is being done
            std::set<std::size_t> hit;        // already hit by this activation (one hit each)
            std::map<std::string, float> cooldowns;
        };

        [[nodiscard]] const rpg::RealtimeTiming &TimingOf(const rpg::ActionIntent &intent) const;
        void Advance(std::size_t index, float seconds);
        void Hit(std::size_t index);
        void Cancel(std::size_t index);

        const rpg::Content &content_;
        rpg::Roster roster_;
        std::vector<Fighter> fighters_;
        int winner_ = -1;
        rpg::IntentLog log_;
        rpg::Events events_;
    };

    // Is `target` within `timing.range` of `from` and inside its facing cone?
    [[nodiscard]] bool InReach(glm::vec2 from, float yaw, glm::vec2 target, const rpg::RealtimeTiming &timing);

    // What a conventional real-time enemy wants this frame.
    struct Decision final
    {
        glm::vec2 move{0.0F};                // direction to walk (unit length or zero)
        float yaw = 0.0F;                    // where to face
        std::optional<rpg::ActionIntent> act; // an action to start (id is 0; the caller sets it)
    };

    // Face and approach the nearest living enemy; attack with the strongest ready skill that
    // reaches; drink a healing item when below 35% health.
    [[nodiscard]] Decision ChooseRealtimeAction(const rpg::Content &content, const RealtimeEncounter &encounter,
                                                std::size_t actor);
}
