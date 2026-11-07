#pragma once

#include <Rules.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace umbra::turn
{
    // Turn-based combat (user decision 2026-11-07): every round, each living fighter acts
    // once, fastest first (derived speed; ties by roster order). Knows nothing about seconds,
    // animation or the real-time controller. Rules are in TurnCombat.md.
    class TurnEncounter final
    {
    public:
        TurnEncounter(const rpg::Content &content, rpg::Roster roster);

        // Whose turn it is; nothing once the encounter is over.
        [[nodiscard]] std::optional<std::size_t> Current() const;
        [[nodiscard]] int Round() const noexcept { return round_; }
        [[nodiscard]] bool IsOver() const noexcept { return winner_ != -1; }
        [[nodiscard]] int Winner() const noexcept { return winner_; }
        [[nodiscard]] const rpg::Roster &GetRoster() const noexcept { return roster_; }

        // The current fighter acts. An intent with neither skill nor item means "wait"
        // (always legal, so a turn can never get stuck). Anything rejected changes nothing.
        rpg::Rejection Submit(const rpg::ActionIntent &intent);

        // Every legal action for the current fighter, in a fixed order, "wait" last.
        // The ids are 0; the caller gives each submitted decision its own id.
        [[nodiscard]] std::vector<rpg::ActionIntent> LegalActions() const;

        // What happened since the last call.
        rpg::Events TakeEvents() { return std::exchange(events_, {}); }

    private:
        void StartRound();
        void NextTurn();

        const rpg::Content &content_;
        rpg::Roster roster_;
        std::vector<std::size_t> order_; // this round's turn order
        std::size_t position_ = 0;       // index into order_
        int round_ = 0;
        int winner_ = -1;
        rpg::IntentLog log_;
        rpg::Events events_;
    };

    // Conventional turn enemy: heal with an item when low, otherwise the strongest affordable
    // attack on the weakest living enemy, otherwise wait. Goes through Submit like the player.
    [[nodiscard]] rpg::ActionIntent ChooseTurnAction(const rpg::Content &content, const TurnEncounter &encounter);
}
