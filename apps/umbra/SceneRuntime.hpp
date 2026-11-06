#pragma once

#include "ModelLibrary.hpp"

#include <DrawList.hpp>
#include <FileWatcher.hpp>
#include <Physics.hpp>
#include <PlayerController.hpp>
#include <World.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <string>

// Owns the live world for one scene file and keeps it in sync with the files on disk.
// See engine/assets/include/Reload.md for the reload rules.
class SceneRuntime final
{
public:
    SceneRuntime(ModelLibrary &models, std::filesystem::path assetRoot, std::string scenePath);

    // Loads the scene file into a fresh world and swaps it in only on success.
    bool Load();
    // Call once per frame, before extracting the draw list. Polls files every 250 ms.
    void Update();
    // Advances player and physics in fixed 1/60 s steps and copies results into the world.
    // A jump request is kept until a fixed step has used it.
    void Simulate(float deltaSeconds, const umbra::gameplay::PlayerInput &input);
    // Camera look from the mouse (radians).
    void Look(float yawDelta, float pitchDelta) { player_.Look(yawDelta, pitchDelta); }
    // The player's orbit camera, or a fixed overview when the scene has no player.
    [[nodiscard]] umbra::core::Camera GetCamera() const;
    // Collider and player outlines for the debug view.
    void AddDebugLines(umbra::core::DrawList &drawList) const;

    [[nodiscard]] umbra::world::World &GetWorld() noexcept { return *world_; }
    [[nodiscard]] umbra::physics::PhysicsWorld &GetPhysics() noexcept { return *physics_; }
    [[nodiscard]] umbra::gameplay::PlayerController &GetPlayer() noexcept { return player_; }

private:
    void Rebind();
    void SpawnPlayer();
    void SyncPlayerVisual();
    void RefreshWatchList();

    ModelLibrary &models_;
    std::filesystem::path assetRoot_;
    std::string scenePath_;
    std::unique_ptr<umbra::world::World> world_;
    std::unique_ptr<umbra::physics::PhysicsWorld> physics_; // bodies for world_'s colliders; replaced together
    double accumulator_ = 0.0;
    umbra::gameplay::PlayerController player_;
    bool pendingJump_ = false;
    umbra::assets::FileWatcher watcher_;
    std::map<std::filesystem::path, std::string> owners_; // watched file -> model path ("" = the scene file)
    umbra::assets::FileWatcher::Clock::time_point nextPoll_{};
};
