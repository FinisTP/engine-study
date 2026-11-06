#include "SceneRuntime.hpp"

#include <Log.hpp>
#include <ModelInstance.hpp>
#include <Area.hpp>
#include <Interaction.hpp>
#include <PhysicsBridge.hpp>
#include <Checkpoint.hpp>
#include <SceneFile.hpp>

#include <set>

SceneRuntime::SceneRuntime(ModelLibrary &models, std::filesystem::path assetRoot, std::string scenePath)
    : models_(models), assetRoot_(std::move(assetRoot)), scenePath_(std::move(scenePath)),
      world_(std::make_unique<umbra::world::World>()), physics_(std::make_unique<umbra::physics::PhysicsWorld>())
{
}

umbra::scene::SceneLoadOptions SceneRuntime::Options()
{
    umbra::scene::SceneLoadOptions options;
    options.assetRoot = assetRoot_;
    options.models = [&](const std::string &path) -> const umbra::assets::Model * {
        const LoadedModel *loaded = models_.Load(path);
        return loaded ? &loaded->model : nullptr;
    };
    return options;
}

void SceneRuntime::Report(const std::vector<umbra::core::Diagnostic> &diagnostics)
{
    problems_.clear();
    for (const umbra::core::Diagnostic &diagnostic : diagnostics)
    {
        const bool error = diagnostic.severity == umbra::core::Severity::Error;
        problems_.push_back((error ? "Error: " : "Warning: ") + diagnostic.field + " " + diagnostic.message);
        if (error)
            umbra::diagnostics::Error(umbra::core::Format(diagnostic));
        else
            umbra::diagnostics::Info(umbra::core::Format(diagnostic));
    }
}

bool SceneRuntime::Load(bool keepPlayer)
{
    // 1. Load into a fresh world; report everything that was found.
    auto fresh = std::make_unique<umbra::world::World>();
    std::vector<umbra::core::Diagnostic> diagnostics;
    const bool loaded = umbra::scene::LoadScene(assetRoot_ / scenePath_, Options(), *fresh, diagnostics);
    Report(diagnostics);

    // 2. Swap only on success (the scene file is still watched either way, so a fix reloads).
    //    A hot reload keeps the player where it was; otherwise it starts at the 'player' marker.
    if (loaded)
    {
        std::optional<umbra::gameplay::SpawnPoint> start;
        if (keepPlayer && player_.IsSpawned())
            start = umbra::gameplay::SpawnPoint{player_.GetFeet(*physics_), player_.GetFacingYaw()};
        Install(std::move(fresh), start);
    }
    else
        umbra::diagnostics::Error("Scene " + scenePath_ + " was not loaded; keeping the current scene.");
    RefreshWatchList();
    return loaded;
}

bool SceneRuntime::ChangeScene(const std::string &scenePath, const std::string &spawnId)
{
    // Same all-or-nothing rule: a broken destination leaves the player where they are.
    auto fresh = std::make_unique<umbra::world::World>();
    std::vector<umbra::core::Diagnostic> diagnostics;
    const bool loaded = umbra::scene::LoadScene(assetRoot_ / scenePath, Options(), *fresh, diagnostics);
    Report(diagnostics);
    if (!loaded)
    {
        umbra::diagnostics::Error("Cannot enter " + scenePath + "; staying in " + scenePath_ + ".");
        return false;
    }
    fresh->UpdateTransforms();
    const std::optional<umbra::gameplay::SpawnPoint> start = umbra::gameplay::FindSpawnPoint(*fresh, spawnId);
    if (!start)
        problems_.push_back("Warning: " + scenePath + " has no spawn '" + spawnId + "'; using 'player'.");
    scenePath_ = scenePath;
    Install(std::move(fresh), start);
    RefreshWatchList();
    return true;
}

bool SceneRuntime::SaveCheckpoint(const std::filesystem::path &directory) const
{
    if (!player_.IsSpawned())
        return false;
    std::vector<umbra::core::Diagnostic> diagnostics;
    const umbra::scene::Checkpoint checkpoint{scenePath_, player_.GetFeet(*physics_), player_.GetFacingYaw()};
    const bool saved = umbra::scene::SaveCheckpoint(*world_, checkpoint, directory, diagnostics);
    for (const umbra::core::Diagnostic &diagnostic : diagnostics)
        umbra::diagnostics::Error(umbra::core::Format(diagnostic));
    return saved;
}

bool SceneRuntime::LoadCheckpoint(const std::filesystem::path &directory)
{
    auto fresh = std::make_unique<umbra::world::World>();
    std::vector<umbra::core::Diagnostic> diagnostics;
    umbra::scene::Checkpoint checkpoint;
    if (!umbra::scene::LoadCheckpoint(directory, Options(), *fresh, checkpoint, diagnostics))
    {
        for (const umbra::core::Diagnostic &diagnostic : diagnostics)
            umbra::diagnostics::Error(umbra::core::Format(diagnostic));
        return false;
    }
    Report(diagnostics);
    scenePath_ = checkpoint.scene; // hot reload and portals continue from that area
    Install(std::move(fresh), umbra::gameplay::SpawnPoint{checkpoint.feet, checkpoint.yaw});
    RefreshWatchList();
    return true;
}

void SceneRuntime::Install(std::unique_ptr<umbra::world::World> fresh, std::optional<umbra::gameplay::SpawnPoint> start)
{
    // 1. The player is a physics character, not a collider: drop a collider authored on it.
    if (fresh->RemoveCollider(fresh->Find("player")))
        umbra::diagnostics::Info("Warning: the 'player' entity's collider is ignored; the player is a character capsule.");

    // 2. The new world gets its own physics world, so old and new bodies never mix.
    //    Replacing physics_ destroys the old bodies and the old character with it.
    auto freshPhysics = std::make_unique<umbra::physics::PhysicsWorld>();
    fresh->UpdateTransforms();
    if (const std::size_t failed = umbra::gameplay::CreateBodies(*fresh, *freshPhysics); failed > 0)
        umbra::diagnostics::Error(std::to_string(failed) + " collider(s) have unusable sizes and got no physics body.");
    world_ = std::move(fresh);
    physics_ = std::move(freshPhysics);
    accumulator_ = 0.0;
    player_.Reset();
    stride_.Reset();
    pendingJump_ = false;
    portalArmed_ = false; // don't bounce straight back if the spawn is inside a portal

    // 3. Player at the given start, else at the 'player' marker (no marker: overview camera).
    if (!start)
        start = umbra::gameplay::FindSpawnPoint(*world_, "player");
    if (start)
        player_.Spawn(*physics_, start->feet, start->yaw);
    Rebind();
    umbra::diagnostics::Info("Loaded scene " + scenePath_ + " with " + std::to_string(world_->Count()) + " entities.");
}

void SceneRuntime::Update()
{
    const auto now = umbra::assets::FileWatcher::Clock::now();
    if (now < nextPoll_)
        return;
    nextPoll_ = now + std::chrono::milliseconds{250};
    const auto changed = watcher_.Poll(now);
    if (changed.empty())
        return;

    // 1. Which models and whether the scene file itself changed.
    bool reloadScene = false;
    std::set<std::string> models;
    for (const auto &file : changed)
    {
        const std::string &owner = owners_[file];
        if (owner.empty())
            reloadScene = true;
        else
            models.insert(owner);
    }

    // 2. Reload models; a node-structure change means instances must be rebuilt from the scene.
    bool rebind = false;
    for (const std::string &model : models)
    {
        const ModelLibrary::ReloadResult result = models_.Reload(model);
        rebind |= result == ModelLibrary::ReloadResult::Updated;
        reloadScene |= result == ModelLibrary::ReloadResult::StructureChanged;
    }

    // 3. Publish before the next frame: new world, or the same world pointing at new GPU data.
    //    If the scene reload fails the old world stays, so its handles must still be refreshed.
    if (reloadScene)
        rebind = !Load();
    if (rebind)
        Rebind();
    RefreshWatchList();
}

SimulationEvents SceneRuntime::Simulate(float deltaSeconds, const umbra::gameplay::PlayerInput &input)
{
    SimulationEvents events;
    pendingJump_ |= input.jump;
    // Fixed steps keep physics stable and repeatable whatever the frame rate.
    // After a long stall (breakpoint, window drag) drop the backlog instead of
    // running hundreds of catch-up steps.
    constexpr double step = 1.0 / 60.0;
    constexpr int maxStepsPerFrame = 5;
    accumulator_ += deltaSeconds;
    int steps = 0;
    while (accumulator_ >= step && steps < maxStepsPerFrame)
    {
        umbra::gameplay::PlayerInput stepInput = input;
        stepInput.jump = pendingJump_;
        pendingJump_ = false;
        const bool wasGrounded = player_.IsSpawned() && player_.IsGrounded(*physics_);
        player_.Update(*physics_, stepInput, static_cast<float>(step));
        if (stepInput.jump && wasGrounded && player_.GetVelocity(*physics_).y > 0.1F)
            events.jumped = true;
        umbra::gameplay::PushKinematic(*world_, *physics_, static_cast<float>(step));
        physics_->Step(static_cast<float>(step));
        umbra::gameplay::PullDynamic(*world_, *physics_);
        world_->UpdateTransforms();
        accumulator_ -= step;
        ++steps;
    }
    if (steps == maxStepsPerFrame)
        accumulator_ = 0.0;
    events.steps = steps;

    // Walk animation follows the character's horizontal speed.
    if (player_.IsSpawned())
    {
        const glm::vec3 velocity = player_.GetVelocity(*physics_);
        const float speed = glm::length(glm::vec2{velocity.x, velocity.z});
        if (stride_.Update(speed, player_.IsGrounded(*physics_), deltaSeconds))
            ++events.footsteps;

        // Portals fire once the player has been outside every portal since arriving.
        const auto &settings = player_.GetSettings();
        const glm::vec3 center = player_.GetFeet(*physics_) + glm::vec3{0.0F, settings.radius + settings.halfHeight, 0.0F};
        const umbra::world::Entity portal = umbra::gameplay::FindPortal(*world_, center);
        if (portal && portalArmed_)
            events.portal = *world_->GetPortal(portal);
        portalArmed_ = !portal;
    }
    SyncPlayerVisual();
    return events;
}

std::vector<std::string> SceneRuntime::GetProblems() const
{
    std::vector<std::string> problems = problems_;
    if (!bindProblem_.empty())
        problems.push_back("Error: " + bindProblem_);
    return problems;
}

const umbra::world::Interactable *SceneRuntime::FocusedInteractable() const
{
    return world_->GetInteractable(umbra::gameplay::FindInteractable(*world_, *physics_, player_));
}

void SceneRuntime::SyncPlayerVisual()
{
    // The 'player' entity is drawn where the capsule is, turned to the facing direction.
    const umbra::world::Entity entity = world_->Find("player");
    if (!entity || !player_.IsSpawned() || world_->GetParent(entity))
        return;
    umbra::world::Transform transform = *world_->GetLocalTransform(entity);
    const auto &settings = player_.GetSettings();
    // Procedural pose on top: bob up, then lean forward around the body's own right axis (-X turned by yaw).
    const umbra::gameplay::BodyPose pose = stride_.GetPose();
    transform.position = player_.GetFeet(*physics_) + glm::vec3{0.0F, settings.radius + settings.halfHeight + pose.bob, 0.0F};
    transform.rotation = glm::angleAxis(player_.GetFacingYaw(), glm::vec3{0.0F, 1.0F, 0.0F}) *
                         glm::angleAxis(pose.lean, glm::vec3{-1.0F, 0.0F, 0.0F});
    world_->SetLocalTransform(entity, transform);
    world_->UpdateTransforms();
}

umbra::core::Camera SceneRuntime::GetCamera() const
{
    if (player_.IsSpawned())
        return player_.GetCamera(*physics_);
    umbra::core::Camera overview;
    overview.position = {0.0F, 4.0F, 8.0F};
    overview.target = {0.0F, 0.0F, -2.0F};
    return overview;
}

void SceneRuntime::AddDebugLines(umbra::core::DrawList &drawList) const
{
    umbra::gameplay::AddDebugLines(*physics_, drawList);
    player_.AddDebugLines(*physics_, drawList);
}

void SceneRuntime::Rebind()
{
    const std::size_t unresolved = umbra::scene::BindRenderables(
        *world_, [&](const std::string &model, std::uint32_t mesh) { return models_.Resolve(model, mesh); });
    bindProblem_.clear();
    if (unresolved > 0)
    {
        bindProblem_ = std::to_string(unresolved) + " renderable(s) have no loaded mesh and are not drawn.";
        umbra::diagnostics::Error(bindProblem_);
    }
}

void SceneRuntime::RefreshWatchList()
{
    owners_ = models_.WatchedFiles();
    owners_[assetRoot_ / scenePath_] = "";
    std::set<std::filesystem::path> files;
    for (const auto &[file, owner] : owners_)
        files.insert(file);
    watcher_.SetFiles(files);
}
