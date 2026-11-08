#pragma once

#include <Conversation.hpp>
#include <EncounterCoordinator.hpp>
#include <Progression.hpp>
#include <SaveGame.hpp>

#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace umbra::game
{
    enum class Phase
    {
        Exploring,
        Talking,
        Fighting,
        Dead // waiting for "load save" / "load autosave"
    };

    struct FightResult final
    {
        bool won = false;
        bool fled = false;
        rpg::Victory victory;
        std::vector<story::QuestUpdate> quests;
    };

    // The player's whole RPG state and the rules that tie story, combat and saves together.
    // Headless: the app drives it from input and the world, tests drive it directly. See GameSession.md.
    class GameSession final
    {
    public:
        GameSession(const rpg::Content &content, std::uint32_t seed);

        void NewGame(); // a fresh hero, no quests, turn-based mode
        [[nodiscard]] Phase GetPhase() const noexcept { return phase_; }
        [[nodiscard]] rpg::Character &Hero() noexcept { return hero_; }
        [[nodiscard]] story::QuestLog &Quests() noexcept { return quests_; }
        [[nodiscard]] const rpg::Content &Content() const noexcept { return content_; }

        // Combat mode is chosen outside combat only (user decision 2026-11-07).
        [[nodiscard]] CombatMode Mode() const noexcept { return mode_; }
        bool ToggleMode();

        // --- Conversations ---
        bool Talk(const std::string &conversation);
        [[nodiscard]] story::Conversation *CurrentConversation() noexcept { return conversation_.get(); }
        story::ChoiceResult Choose(std::size_t choice);
        void EndTalk(); // walking away, or the last line was read

        // --- Fights ---
        // The touched enemy plus 0..maxSize-1 more picked at random from the area's pool.
        [[nodiscard]] std::vector<std::string> PickGroup(const std::string &touched, const std::vector<std::string> &pool, int maxSize);
        bool StartFight(const std::vector<std::string> &enemyActors);
        [[nodiscard]] encounter::EncounterCoordinator &Combat() noexcept { return combat_; }
        [[nodiscard]] std::uint64_t FightId() const noexcept { return fightId_; }
        [[nodiscard]] std::uint64_t NextDecisionId() noexcept { return ++decisionId_; }
        [[nodiscard]] std::vector<rpg::Character> &Enemies() noexcept { return enemies_; }
        // Fleeing always works (user decision 2026-11-07).
        bool Flee();
        // Call after every fight update. Returns the result once when the fight has ended.
        std::optional<FightResult> CheckFightEnd();

        // --- Saves (outside combat only) ---
        [[nodiscard]] std::optional<SaveData> MakeSave(const std::string &scene, glm::vec3 feet, float yaw) const;
        void ApplySave(const SaveData &data);

        // Quest changes since the last call, for the HUD.
        std::vector<story::QuestUpdate> TakeQuestUpdates() { return std::exchange(updates_, {}); }

    private:
        void Notify(const story::GameEvent &event);

        const rpg::Content &content_;
        std::mt19937 random_; // only for world choices like enemy groups, never inside the rules
        Phase phase_ = Phase::Exploring;
        rpg::Character hero_;
        story::QuestLog quests_;
        CombatMode mode_ = CombatMode::Turn;
        std::unique_ptr<story::Conversation> conversation_;
        encounter::EncounterCoordinator combat_;
        std::vector<rpg::Character> enemies_;
        std::uint64_t fightId_ = 0;
        std::uint64_t decisionId_ = 0;
        std::vector<story::QuestUpdate> updates_;
    };
}
