#pragma once

#include "SceneRuntime.hpp"

#include <DrawList.hpp>
#include <GameSession.hpp>
#include <Input.hpp>

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
    GameFlow(const umbra::rpg::Content &content, SceneRuntime &scene, std::filesystem::path saveFolder,
             std::string startScene);

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

    umbra::game::GameSession session_;
    SceneRuntime &scene_;
    umbra::game::SaveSlots slots_;
    std::string startScene_;
    int selected_ = 0;                 // menu cursor
    std::vector<std::size_t> fighters_; // roamer index of each enemy (roster index - 1)
    int fightGeneration_ = 0;          // the world the fight started in
    float enemyDelay_ = 0.0F;          // turn fights: pause before an enemy acts
    float contactCooldown_ = 0.0F;     // after fleeing, touching an enemy does nothing for a moment
    std::vector<std::string> log_;     // recent fight lines
    std::string message_;
};
