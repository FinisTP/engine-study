#include "SceneRuntime.hpp"

#include <Log.hpp>
#include <ModelInstance.hpp>
#include <Interaction.hpp>
#include <PhysicsBridge.hpp>
#include <SceneFile.hpp>

#include <set>

SceneRuntime::SceneRuntime(ModelLibrary &models, std::filesystem::path assetRoot, std::string scenePath)
    : models_(models), assetRoot_(std::move(assetRoot)), scenePath_(std::move(scenePath)),
      world_(std::make_unique<umbra::world::World>()), physics_(std::make_unique<umbra::physics::PhysicsWorld>())
{
}

bool SceneRuntime::Load(bool keepPlayer)
{
    umbra::scene::SceneLoadOptions options;
    options.assetRoot = assetRoot_;
    options.models = [&](const std::string &path) -> const umbra::assets::Model * {
        const LoadedModel *loaded = models_.Load(path);
        return loaded ? &loaded->model : nullptr;
    };

    // 1. Load into a fresh world; report everything that was found.
    auto fresh = std::make_unique<umbra::world::World>();
    std::vector<umbra::core::Diagnostic> diagnostics;
    const bool loaded = umbra::scene::LoadScene(assetRoot_ / scenePath_, options, *fresh, diagnostics);
    problems_.clear();
    for (const umbra::core::Diagnostic &diagnostic : diagnostics)
    {
        problems_.push_back((diagnostic.severity == umbra::core::Severity::Error ? "Error: " : "Warning: ") +
                            diagnostic.field + " " + diagnostic.message);
        if (diagnostic.severity == umbra::core::Severity::Error)
            umbra::diagnostics::Error(umbra::core::Format(diagnostic));
        else
            umbra::diagnostics::Info(umbra::core::Format(diagnostic));
    }

    // 2. Swap only on success (the scene file is still watched either way, so a fix reloads).
    //    The new world gets its own physics world, so old and new bodies never mix.
    if (loaded)
    {
        // The player is a physics character, not a collider: drop a collider authored on it.
        if (fresh->RemoveCollider(fresh->Find("player")))
            umbra::diagnostics::Info("Warning: the 'player' entity's collider is ignored; the player is a character capsule.");
        auto freshPhysics = std::make_unique<umbra::physics::PhysicsWorld>();
        fresh->UpdateTransforms();
        if (const std::size_t failed = umbra::gameplay::CreateBodies(*fresh, *freshPhysics); failed > 0)
            umbra::diagnostics::Error(std::to_string(failed) + " collider(s) have unusable sizes and got no physics body.");
        // Keep the player where it was on a hot reload; spawn at the marker otherwise.
        keepPlayer = keepPlayer && player_.IsSpawned();
        const glm::vec3 feet = keepPlayer ? player_.GetFeet(*physics_) : glm::vec3{0.0F};
        world_ = std::move(fresh);
        physics_ = std::move(freshPhysics);
        accumulator_ = 0.0;
        player_.Reset(); // the old character died with the old physics world
        stride_.Reset();
        pendingJump_ = false;
        if (keepPlayer)
            player_.Spawn(*physics_, feet, player_.GetFacingYaw());
        else
            SpawnPlayer();
        Rebind();
        umbra::diagnostics::Info("Loaded scene " + scenePath_ + " with " + std::to_string(world_->Count()) + " entities.");
    }
    else
        umbra::diagnostics::Error("Scene " + scenePath_ + " was not loaded; keeping the current scene.");
    RefreshWatchList();
    return loaded;
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

    // Walk animation follows the character's horizontal speed.
    if (player_.IsSpawned())
    {
        const glm::vec3 velocity = player_.GetVelocity(*physics_);
        const float speed = glm::length(glm::vec2{velocity.x, velocity.z});
        if (stride_.Update(speed, player_.IsGrounded(*physics_), deltaSeconds))
            ++events.footsteps;
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

void SceneRuntime::SpawnPlayer()
{
    // The 'player' entity marks the spawn: its position is the feet, its rotation the facing.
    const umbra::world::Entity marker = world_->Find("player");
    if (!marker)
        return;
    glm::vec3 position, scale;
    glm::quat rotation;
    umbra::gameplay::Decompose(*world_->GetWorldMatrix(marker), position, rotation, scale);
    const glm::vec3 forward = rotation * glm::vec3{0.0F, 0.0F, -1.0F};
    player_.Spawn(*physics_, position, std::atan2(-forward.x, -forward.z));
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
