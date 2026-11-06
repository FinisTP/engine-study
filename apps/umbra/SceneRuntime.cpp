#include "SceneRuntime.hpp"

#include <Log.hpp>
#include <ModelInstance.hpp>
#include <PhysicsBridge.hpp>
#include <SceneFile.hpp>

#include <set>

SceneRuntime::SceneRuntime(ModelLibrary &models, std::filesystem::path assetRoot, std::string scenePath)
    : models_(models), assetRoot_(std::move(assetRoot)), scenePath_(std::move(scenePath)),
      world_(std::make_unique<umbra::world::World>()), physics_(std::make_unique<umbra::physics::PhysicsWorld>())
{
}

bool SceneRuntime::Load()
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
    for (const umbra::core::Diagnostic &diagnostic : diagnostics)
    {
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
        const bool keepPlayer = player_.IsSpawned();
        const glm::vec3 feet = keepPlayer ? player_.GetFeet(*physics_) : glm::vec3{0.0F};
        world_ = std::move(fresh);
        physics_ = std::move(freshPhysics);
        accumulator_ = 0.0;
        player_.Reset(); // the old character died with the old physics world
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

void SceneRuntime::Simulate(float deltaSeconds, const umbra::gameplay::PlayerInput &input)
{
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
        player_.Update(*physics_, stepInput, static_cast<float>(step));
        umbra::gameplay::PushKinematic(*world_, *physics_, static_cast<float>(step));
        physics_->Step(static_cast<float>(step));
        umbra::gameplay::PullDynamic(*world_, *physics_);
        world_->UpdateTransforms();
        accumulator_ -= step;
        ++steps;
    }
    if (steps == maxStepsPerFrame)
        accumulator_ = 0.0;
    SyncPlayerVisual();
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
    transform.position = player_.GetFeet(*physics_) + glm::vec3{0.0F, settings.radius + settings.halfHeight, 0.0F};
    transform.rotation = glm::angleAxis(player_.GetFacingYaw(), glm::vec3{0.0F, 1.0F, 0.0F});
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
    if (unresolved > 0)
        umbra::diagnostics::Error(std::to_string(unresolved) + " renderable(s) have no loaded mesh and are not drawn.");
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
