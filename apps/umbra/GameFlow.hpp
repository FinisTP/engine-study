#pragma once

#include "SceneRuntime.hpp"

#include <DrawList.hpp>
#include <FileWatcher.hpp>
#include <GameSession.hpp>
#include <Input.hpp>
#include <PolicyDecider.hpp>

#include <map>
#include <memory>

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// Ties the RPG session (story, fights, saves) to the running world and the keyboard.
// The rules live in umbra::game::GameSession; this only reads keys, moves enemies and fills the
// screen. See GameFlow.md.
class GameFlow final
{
public:
    // `content` is the live RPG content loaded from `contentFile`; it is replaced in place when
    // the file changes and the new version fits the running game (see GameFlow.md, "Reloading").
    // `offerContinue`: at launch, if a save exists, show "Continue" before playing (not when a
    // specific scene was asked for, e.g. the editor's Play).
    GameFlow(umbra::rpg::Content &content, std::filesystem::path contentFile, SceneRuntime &scene,
             std::filesystem::path saveFolder, std::string startScene, bool offerContinue = false);

    // Once per frame, before Simulate: menus, fights and touching an enemy.
    void Update(const umbra::platform::Input &input, float deltaSeconds);
    [[nodiscard]] bool IsExploring() const { return session_.GetPhase() == umbra::game::Phase::Exploring; }
    // May the player walk? Not while talking, in a turn fight or dead.
    [[nodiscard]] bool PlayerCanMove() const;
    // Portals work while exploring or talking, never in a fight or after death.
    [[nodiscard]] bool AllowsPortals() const;
    // Esc: walks away from a conversation, does nothing on the death menu. False = open the pause menu.
    bool HandleEscape();
    // E on something: true if it was a conversation or a save stone (handled here).
    bool Interact(const umbra::world::Interactable &interactable);
    // A portal brought the player to a new area: autosave there.
    void OnAreaEntered();
    void FillHud(umbra::core::Hud &hud);
    // News for the screen (quest changes, saves, fight results); empty when nothing new.
    std::string TakeMessage() { return std::exchange(message_, {}); }

private:
    void UpdateExploring(const umbra::platform::Input &input);
    void UpdateTalking(const umbra::platform::Input &input);
    void UpdateTurnFight(const umbra::platform::Input &input, float deltaSeconds);
    void UpdateRealtimeFight(const umbra::platform::Input &input, float deltaSeconds);
    void UpdateDeath(const umbra::platform::Input &input);
    void UpdateStartMenu(const umbra::platform::Input &input);
    [[nodiscard]] std::vector<std::string> StartMenuItems() const;
    void ReloadContent();
    void LoadPolicies();

    void StartFight(std::size_t roamer);
    void Flee();
    void EndFight(const umbra::game::FightResult &result);
    void Act(umbra::rpg::ActionIntent intent, bool player);
    void ReadEvents(const umbra::rpg::Events &events);

    bool Save(const std::filesystem::path &file);
    bool Load(const std::filesystem::path &file);
    // W/S or arrows move the cursor over `count` items; true when Enter picks one.
    bool MenuInput(const umbra::platform::Input &input, std::size_t count);

    [[nodiscard]] const umbra::rpg::Roster *FightRoster();
    [[nodiscard]] std::string FighterName(std::size_t index);
    [[nodiscard]] std::string ActionName(const umbra::rpg::ActionIntent &intent);
    [[nodiscard]] std::vector<std::string> RealtimeSkills();
    [[nodiscard]] std::string PotionItem();
    void Say(const std::string &text);

    umbra::rpg::Content &content_;
    std::filesystem::path contentFile_;
    umbra::assets::FileWatcher contentWatcher_;
    bool reloadPending_ = false;
    umbra::game::GameSession session_;
    SceneRuntime &scene_;
    umbra::game::SaveSlots slots_;
    std::string startScene_;
    bool startMenu_ = false;           // "Continue / New game" shown at launch when a save exists
    int selected_ = 0;                 // menu cursor
    std::size_t touched_ = 0;          // the dungeon roamer that started the fight
    std::vector<std::size_t> fighters_; // arena roamer index of each enemy (roster index - 1)
    int fightGeneration_ = 0;          // the world the fight started in
    float enemyDelay_ = 0.0F;          // turn fights: pause before an enemy acts
    float contactCooldown_ = 0.0F;     // after fleeing, touching an enemy does nothing for a moment
    std::vector<std::string> log_;     // recent fight lines
    std::string message_;

    // Learned turn-combat AI per actor (game/policy). Each has its own worker thread.
    struct LearnedEnemy final
    {
        umbra::policy::PolicyModel model;
        std::unique_ptr<umbra::policy::PolicyDecider> decider;
    };
    std::map<std::string, LearnedEnemy> learned_;
    std::size_t policyFor_ = static_cast<std::size_t>(-1); // roster index whose decision is pending
    std::uint64_t policyRequest_ = 0;
    std::vector<std::uint8_t> policyMask_;
};
