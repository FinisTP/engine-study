#pragma once

#include "GameComponents.hpp"
#include "ModelLibrary.hpp"

#include <DrawList.hpp>
#include <FileWatcher.hpp>
#include <Physics.hpp>
#include <Area.hpp>
#include <PlayerController.hpp>
#include <Roamer.hpp>
#include <SceneFile.hpp>
#include <StrideAnimator.hpp>
#include <World.hpp>

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

// What happened during one Simulate call (for sounds).
struct SimulationEvents final
{
    int steps = 0; // fixed physics steps run
    int footsteps = 0;
    bool jumped = false;
    std::optional<umbra::world::Portal> portal; // the player walked into this portal
};

// How a roamer is drawn: a model mesh, a tint and a size. The model is centered and its largest
// half-extent is 1 (like the -1..1 sample cube), so `scale` is the half-size in meters.
struct RoamerLook final
{
    std::string model = "scenes/phase2.gltf";
    std::uint32_t mesh = 0;
    glm::vec4 color{0.9F, 0.5F, 0.1F, 1.0F};
    glm::vec3 scale{0.35F, 0.8F, 0.35F};
};

// One dungeon enemy walking in the world, and the entity that draws it.
struct RoamerSlot final
{
    umbra::gameplay::Roamer roamer;
    umbra::world::Entity visual;
    std::vector<std::string> pool; // its spawner's pool and group size, for the fight it starts
    int group = 1;
    bool alive = true;                  // false once removed (indices stay valid)
    std::optional<glm::vec3> order;     // a velocity chosen by the fight; replaces wandering
    float yaw = 0.0F;                   // facing, 0 faces -z
    glm::vec3 scale{1.0F};              // visual scale
    float lift = 0.0F;                  // feet -> model center (so the model stands on the ground)
};

// Owns the live world for one scene file and keeps it in sync with the files on disk.
// See engine/assets/include/Reload.md for the reload rules.
class SceneRuntime final
{
public:
    SceneRuntime(ModelLibrary &models, umbra::assets::AssetFolders assetRoot, std::string scenePath);

    // Loads the scene file into a fresh world and swaps it in only on success.
    // keepPlayer: a hot reload keeps the player where it is; false respawns at the 'player' marker.
    bool Load(bool keepPlayer = true);
    // Reloads the area from disk and puts the player back at the start.
    bool Restart() { return Load(false); }
    // Loads another area and puts the player at the entity `spawnId` there (portals).
    // On failure the current area stays.
    bool ChangeScene(const std::string &scenePath, const std::string &spawnId);
    // Development checkpoint of the whole world and the player (engine/scene/include/Checkpoint.hpp).
    bool SaveCheckpoint(const std::filesystem::path &directory) const;
    bool LoadCheckpoint(const std::filesystem::path &directory);
    [[nodiscard]] const std::string &GetScenePath() const noexcept { return scenePath_; }
    // Call once per frame, before extracting the draw list. Polls files every 250 ms.
    void Update();
    // Advances player and physics in fixed 1/60 s steps and copies results into the world.
    // A jump request is kept until a fixed step has used it.
    SimulationEvents Simulate(float deltaSeconds, const umbra::gameplay::PlayerInput &input);
    // Camera look from the mouse (radians).
    void Look(float yawDelta, float pitchDelta) { player_.Look(yawDelta, pitchDelta); }
    // The player's orbit camera, or a fixed overview when the scene has no player.
    [[nodiscard]] umbra::core::Camera GetCamera() const;
    // The interactable the player is facing within reach, or nullptr.
    [[nodiscard]] const umbra::world::Interactable *FocusedInteractable() const;
    // Problems found by the last load (missing assets, bad fields), formatted for the screen.
    [[nodiscard]] std::vector<std::string> GetProblems() const;
    // Collider and player outlines for the debug view.
    void AddDebugLines(umbra::core::DrawList &drawList) const;

    // --- Player and dungeon enemies (P6). Roamers are rebuilt from the spawners on every load. ---
    [[nodiscard]] umbra::gameplay::SpawnPoint GetPlayerSpot() const;
    // Loads an area and puts the player at an exact spot (loading a save).
    bool ChangeSceneAt(const std::string &scenePath, umbra::gameplay::SpawnPoint spot);
    // Which model each enemy actor uses (the game sets this from the RPG content); nothing = the cube.
    void SetRoamerLook(std::function<std::optional<RoamerLook>(const std::string &actor)> look) { roamerLook_ = std::move(look); }
    // Wandering and chasing happen only while active (not in fights, menus or death).
    void SetRoamersActive(bool active) noexcept { roamersActive_ = active; }
    // A living roamer touching the player, if any.
    [[nodiscard]] std::optional<std::size_t> TouchingRoamer() const;
    [[nodiscard]] std::vector<RoamerSlot> &Roamers() noexcept { return roamers_; }
    [[nodiscard]] glm::vec3 RoamerFeet(std::size_t index) const;
    // A new roamer standing at `feet`, wandering within homeRadius of home. Returns its index.
    std::size_t AddRoamer(const std::string &actor, glm::vec3 feet, glm::vec3 home, float homeRadius, bool chase);
    void RemoveRoamer(std::size_t index);
    // Changes whenever the world is replaced (load, restart, portal), so fights can tell.
    [[nodiscard]] int Generation() const noexcept { return generation_; }
    // The live scene's "music" component (third-person plugin): track and volume; empty track = none.
    [[nodiscard]] std::pair<std::string, float> Music() const;

    // --- Battle arena: fights happen in their own scene (user decision 2026-11-07) ---
    // Loads the arena and keeps the current area, untouched, to come back to.
    // False if the arena doesn't load (nothing changes) or an arena is already open.
    bool EnterArena(const std::string &arenaScene);
    // Back to the kept area exactly as it was: player spot, enemies, physics. False if none is kept.
    bool LeaveArena();
    [[nodiscard]] bool InArena() const noexcept { return kept_ != nullptr; }
    [[nodiscard]] std::optional<umbra::gameplay::SpawnPoint> FindMarker(const std::string &id) const
    {
        return umbra::gameplay::FindSpawnPoint(*world_, id);
    }
    // Hit feedback: the entity turns red for a moment.
    void Flash(umbra::world::Entity entity);

    [[nodiscard]] umbra::world::World &GetWorld() noexcept { return *world_; }
    [[nodiscard]] umbra::physics::PhysicsWorld &GetPhysics() noexcept { return *physics_; }
    [[nodiscard]] umbra::gameplay::PlayerController &GetPlayer() noexcept { return player_; }

private:
    [[nodiscard]] umbra::scene::SceneLoadOptions Options();
    void Report(const std::vector<umbra::core::Diagnostic> &diagnostics);
    // Makes `fresh` the live world with new physics; the player starts at `start` or the 'player' marker.
    void Install(std::unique_ptr<umbra::world::World> fresh, std::optional<umbra::gameplay::SpawnPoint> start);
    void Rebind();
    void SyncPlayerVisual();
    void RefreshWatchList();
    [[nodiscard]] std::unique_ptr<umbra::world::World> LoadFresh(const std::string &scenePath);
    void SpawnRoamers();
    void UpdateRoamers(float step);
    void SyncRoamerVisuals();

    ModelLibrary &models_;
    umbra::assets::AssetFolders assetRoot_; // project, then plugins
    std::string scenePath_;
    std::unique_ptr<umbra::world::World> world_;
    std::unique_ptr<umbra::physics::PhysicsWorld> physics_; // bodies for world_'s colliders; replaced together
    double accumulator_ = 0.0;
    umbra::gameplay::PlayerController player_;
    bool pendingJump_ = false;
    bool portalArmed_ = false;
    umbra::gameplay::StrideAnimator stride_;
    std::vector<std::string> problems_; // from the last Load
    std::string bindProblem_;           // from the last Rebind
    umbra::assets::FileWatcher watcher_;
    std::map<std::filesystem::path, std::string> owners_; // watched file -> model path ("" = the scene file)
    umbra::assets::FileWatcher::Clock::time_point nextPoll_{};
    std::vector<RoamerSlot> roamers_;
    umbra::game::ComponentSet components_; // project components of world_ (GameComponents.hpp)
    // The area waiting while a fight runs in the arena.
    struct KeptArea final
    {
        std::unique_ptr<umbra::world::World> world;
        std::unique_ptr<umbra::physics::PhysicsWorld> physics;
        std::vector<RoamerSlot> roamers;
        umbra::game::ComponentSet components;
        std::string scenePath;
        umbra::gameplay::SpawnPoint player;
    };
    std::unique_ptr<KeptArea> kept_;
    struct FlashState final
    {
        umbra::world::Entity entity;
        glm::vec4 color; // to restore
        float secondsLeft = 0.0F;
    };
    std::vector<FlashState> flashes_;
    void UpdateFlashes(float seconds);
    std::mt19937 roamerRandom_{7};
    std::function<std::optional<RoamerLook>(const std::string &)> roamerLook_;
    bool roamersActive_ = true;
    int generation_ = 0;
};
