#include "SceneRuntime.hpp"

#include <AnimGraph.hpp>
#include <Vfx.hpp>

#include <Log.hpp>
#include <ModelInstance.hpp>
#include <Area.hpp>
#include <Interaction.hpp>
#include <PhysicsBridge.hpp>
#include <Checkpoint.hpp>
#include <SceneFile.hpp>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <set>

namespace
{
    // Placeholder look for enemies: the sample cube, tinted per actor.
    glm::vec4 RoamerColor(const std::string &actor)
    {
        if (actor == "wolf")
            return {0.55F, 0.55F, 0.6F, 1.0F};
        if (actor == "bandit")
            return {0.75F, 0.25F, 0.2F, 1.0F};
        if (actor == "bandit_leader")
            return {0.5F, 0.2F, 0.65F, 1.0F};
        return {0.9F, 0.5F, 0.1F, 1.0F};
    }
}

SceneRuntime::SceneRuntime(ModelLibrary &models, umbra::assets::AssetFolders assetRoot, std::string scenePath)
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
        return loaded ? loaded->model.get() : nullptr;
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

std::unique_ptr<umbra::world::World> SceneRuntime::LoadFresh(const std::string &scenePath)
{
    // Same all-or-nothing rule as Load: a broken destination leaves the player where they are.
    auto fresh = std::make_unique<umbra::world::World>();
    std::vector<umbra::core::Diagnostic> diagnostics;
    const bool loaded = umbra::scene::LoadScene(assetRoot_ / scenePath, Options(), *fresh, diagnostics);
    Report(diagnostics);
    if (!loaded)
    {
        umbra::diagnostics::Error("Cannot enter " + scenePath + "; staying in " + scenePath_ + ".");
        return nullptr;
    }
    fresh->UpdateTransforms();
    return fresh;
}

bool SceneRuntime::ChangeScene(const std::string &scenePath, const std::string &spawnId)
{
    std::unique_ptr<umbra::world::World> fresh = LoadFresh(scenePath);
    if (!fresh)
        return false;
    const std::optional<umbra::gameplay::SpawnPoint> start = umbra::gameplay::FindSpawnPoint(*fresh, spawnId);
    if (!start)
        problems_.push_back("Warning: " + scenePath + " has no spawn '" + spawnId + "'; using 'player'.");
    scenePath_ = scenePath;
    kept_.reset(); // leaving for another area: an open arena's kept area is dropped
    Install(std::move(fresh), start);
    RefreshWatchList();
    return true;
}

bool SceneRuntime::ChangeSceneAt(const std::string &scenePath, umbra::gameplay::SpawnPoint spot)
{
    std::unique_ptr<umbra::world::World> fresh = LoadFresh(scenePath);
    if (!fresh)
        return false;
    scenePath_ = scenePath;
    kept_.reset();
    Install(std::move(fresh), spot);
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
    // Roamer cubes were saved with the world; drop them, the spawners make fresh ones.
    std::vector<umbra::world::Entity> roamerVisuals;
    fresh->ForEach([&](umbra::world::Entity entity) {
        if (fresh->GetId(entity)->rfind("roamer_", 0) == 0)
            roamerVisuals.push_back(entity);
    });
    for (umbra::world::Entity entity : roamerVisuals)
        fresh->Destroy(entity);
    scenePath_ = checkpoint.scene; // hot reload and portals continue from that area
    kept_.reset();
    Install(std::move(fresh), umbra::gameplay::SpawnPoint{checkpoint.feet, checkpoint.yaw});
    RefreshWatchList();
    return true;
}

void SceneRuntime::Install(std::unique_ptr<umbra::world::World> fresh, std::optional<umbra::gameplay::SpawnPoint> start)
{
    // 0. One clock for the whole game (ADR 0010, S6): a running day keeps its time across areas.
    if (world_ && world_->GetEnvironment().dayLength > 0.0F && fresh->GetEnvironment().dayLength > 0.0F)
        fresh->SetHour(world_->Hour());

    // 1. The player is a physics character, not a collider: drop a collider authored on it.
    if (fresh->RemoveCollider(fresh->Find("player")))
        umbra::diagnostics::Info("Warning: the 'player' entity's collider is ignored; the player is a character capsule.");

    // 2. The new world gets its own physics world, so old and new bodies never mix.
    //    Replacing physics_ destroys the old bodies and the old character with it.
    auto freshPhysics = std::make_unique<umbra::physics::PhysicsWorld>();
    fresh->UpdateTransforms();
    if (const std::size_t failed = umbra::gameplay::CreateBodies(*fresh, *freshPhysics); failed > 0)
        umbra::diagnostics::Error(std::to_string(failed) + " collider(s) have unusable sizes and got no physics body.");
    roamers_.clear(); // their characters live in the old physics world, destroyed below
    flashes_.clear();
    fading_.clear(); // their entities go with the old world
    particles_.Clear();
    attachedEffects_.clear();
    clipEvents_.clear();
    world_ = std::move(fresh);
    physics_ = std::move(freshPhysics);
    ++generation_;
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
    SpawnRoamers();
    // 4. Project components get their C++ behaviour (a type without one is only data).
    for (const std::string &type : components_.Create(*world_))
        umbra::diagnostics::Info("Warning: component '" + type + "' has no C++ behaviour in this build; it does nothing.");
    world_->UpdateTransforms();
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
        else if (owner.ends_with(".anim.json"))
            graphs_.erase(owner); // read again by the rebind below
        else if (owner.ends_with(".vfx.json"))
        {
            effects_.erase(owner); // read again when next used; running copies end
            particles_.Clear();
        }
        else
            models.insert(owner);
    }
    bool rebind = std::any_of(changed.begin(), changed.end(), [&](const auto &file) { return owners_[file].ends_with(".anim.json"); });

    // 2. Reload models; a node-structure change means instances must be rebuilt from the scene.
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
        UpdateRoamers(static_cast<float>(step));
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
        // A rigged player's footsteps come from its walk clip's events instead (S9).
        if (stride_.Update(speed, player_.IsGrounded(*physics_), deltaSeconds) && world_->GetAnimator(world_->Find("player")) == nullptr)
            ++events.footsteps;

        // Portals fire once the player has been outside every portal since arriving.
        const auto &settings = player_.GetSettings();
        const glm::vec3 center = player_.GetFeet(*physics_) + glm::vec3{0.0F, settings.radius + settings.halfHeight, 0.0F};
        const umbra::world::Entity portal = umbra::gameplay::FindPortal(*world_, center);
        if (portal && portalArmed_)
            events.portal = *world_->GetPortal(portal);
        portalArmed_ = !portal;
    }
    components_.Update(*world_, static_cast<float>(deltaSeconds));
    world_->UpdateTransforms();
    SyncPlayerVisual();
    SyncRoamerVisuals();
    UpdateFlashes(deltaSeconds);
    UpdateAnimation(deltaSeconds, events);
    UpdateEffects(static_cast<float>(deltaSeconds));
    return events;
}

std::vector<std::string> SceneRuntime::GetProblems() const
{
    std::vector<std::string> problems = problems_;
    if (!bindProblem_.empty())
        problems.push_back("Error: " + bindProblem_);
    for (const std::string &problem : graphProblems_)
        problems.push_back(problem);
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
    // A rigged player's clips do the walking, so no procedural bob or lean then (S8).
    const umbra::gameplay::BodyPose pose = world_->GetAnimator(entity) ? umbra::gameplay::BodyPose{} : stride_.GetPose();
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
        *world_, [&](const std::string &model, std::uint32_t mesh) { return models_.Resolve(model, mesh); },
        [&](const std::string &material) { return models_.ResolveMaterial(material); });
    bindProblem_.clear();
    if (unresolved > 0)
    {
        bindProblem_ = std::to_string(unresolved) + " renderable(s) have no loaded mesh and are not drawn.";
        umbra::diagnostics::Error(bindProblem_);
    }
    // Animators for rigged models (S8). A rebind starts them again from their first state.
    std::vector<umbra::core::Diagnostic> problems;
    umbra::scene::BindAnimators(
        *world_, [&](const std::string &model) { return models_.Shared(model); }, [&](const std::string &graph) { return LoadGraph(graph); },
        problems);
    graphProblems_.clear();
    for (const umbra::core::Diagnostic &d : problems)
    {
        graphProblems_.push_back(umbra::core::Format(d));
        umbra::diagnostics::Error(graphProblems_.back());
    }
}

std::shared_ptr<const umbra::core::AnimGraph> SceneRuntime::LoadGraph(const std::string &path)
{
    if (const auto it = graphs_.find(path); it != graphs_.end())
        return it->second;
    // A bad graph is remembered as nullptr until its file changes.
    std::vector<umbra::core::Diagnostic> problems;
    const auto graph = umbra::scene::LoadAnimGraph(assetRoot_ / path, path, problems);
    for (const umbra::core::Diagnostic &d : problems)
        umbra::diagnostics::Error(umbra::core::Format(d));
    graphs_[path] = graph;
    return graph;
}

std::optional<glm::vec3> SceneRuntime::ChestOf(umbra::world::Entity entity) const
{
    // An enemy: 60% up its own height (a wolf is low, a bandit tall). Anyone else: 1 m up.
    for (const RoamerSlot &slot : roamers_)
        if (slot.visual == entity)
            return RoamerFeet(static_cast<std::size_t>(&slot - roamers_.data())) + glm::vec3{0.0F, slot.height * 0.6F, 0.0F};
    const std::optional<glm::vec3> at = PositionOf(entity);
    return at ? std::optional<glm::vec3>(*at + glm::vec3{0.0F, 1.0F, 0.0F}) : std::nullopt;
}

std::optional<glm::vec3> SceneRuntime::PositionOf(umbra::world::Entity entity) const
{
    const glm::mat4 *matrix = world_->GetWorldMatrix(entity);
    return matrix ? std::optional<glm::vec3>(glm::vec3((*matrix)[3])) : std::nullopt;
}

std::shared_ptr<const umbra::core::VfxEffect> SceneRuntime::LoadEffect(const std::string &path)
{
    if (const auto it = effects_.find(path); it != effects_.end())
        return it->second;
    // A bad effect is remembered as nullptr (and shown with the scene's problems) until its file changes.
    std::vector<umbra::core::Diagnostic> problems;
    const auto effect = umbra::scene::LoadVfx(assetRoot_ / path, path, problems);
    for (const umbra::core::Diagnostic &d : problems)
    {
        graphProblems_.push_back(umbra::core::Format(d));
        umbra::diagnostics::Error(graphProblems_.back());
    }
    effects_[path] = effect;
    RefreshWatchList(); // its file is watched from now on
    return effect;
}

void SceneRuntime::SpawnEffect(const std::string &effect, glm::vec3 position)
{
    if (!effect.empty())
        particles_.Spawn(LoadEffect(effect), position);
}

void SceneRuntime::UpdateEffects(float seconds)
{
    // 1. Effects that live on entities: "vfx" components, then the game's own (statuses).
    const auto finder = [&](const std::string &path) { return LoadEffect(path); };
    std::vector<std::uint64_t> keys = umbra::scene::SyncVfx(*world_, particles_, finder);
    for (const auto &[entity, path] : attachedEffects_)
        if (const auto at = ChestOf(entity))
        {
            // Slot 1..255 from the path, so one entity can carry several effects.
            const std::uint64_t key = umbra::scene::VfxKey(entity, 1 + static_cast<std::uint32_t>(std::hash<std::string>{}(path) % 255));
            particles_.Attach(key, LoadEffect(path), *at);
            keys.push_back(key);
        }
    particles_.KeepOnly(keys);
    // 2. Every particle moves on.
    particles_.Update(seconds);
}

void SceneRuntime::UpdateAnimation(float seconds, SimulationEvents &events)
{
    // 1. Speeds and flags the graphs choose states from.
    if (umbra::core::Animator *animator = world_->GetAnimator(world_->Find("player")); animator && player_.IsSpawned())
    {
        const glm::vec3 velocity = player_.GetVelocity(*physics_);
        animator->SetNumber("speed", glm::length(glm::vec2{velocity.x, velocity.z}));
    }
    for (const RoamerSlot &slot : roamers_)
        if (umbra::core::Animator *animator = slot.alive ? world_->GetAnimator(slot.visual) : nullptr)
            animator->SetNumber("speed", slot.speed);
    // 2. Bodies of defeated enemies go once their death clip has played.
    for (Fading &body : fading_)
        if ((body.secondsLeft -= seconds) <= 0.0F)
            world_->Destroy(body.entity);
    std::erase_if(fading_, [](const Fading &body) { return body.secondsLeft <= 0.0F; });
    // 3. Advance every animator (poses for this frame's draw list).
    world_->UpdateAnimation(seconds);
    // 4. Clip events (AnimGraph.md): a sound, quieter with distance from the player; an effect at the
    //    entity's feet; the name for the game (a "hit" lands the blow).
    const glm::vec3 listener = player_.IsSpawned() ? player_.GetFeet(*physics_) : glm::vec3{0.0F};
    for (const umbra::world::World::AnimationEvent &e : world_->TakeAnimationEvents())
    {
        const glm::vec3 at = PositionOf(e.entity).value_or(listener);
        constexpr float fullVolumeMeters = 4.0F, silentMeters = 22.0F;
        const float volume = std::clamp((silentMeters - glm::distance(at, listener)) / (silentMeters - fullVolumeMeters), 0.0F, 1.0F);
        if (!e.event.sound.empty() && volume > 0.0F)
            events.sounds.emplace_back(e.event.sound, volume);
        SpawnEffect(e.event.effect, at);
        clipEvents_.emplace_back(e.entity, e.event.name);
    }
}

void SceneRuntime::ClearFading()
{
    for (const Fading &body : fading_)
        world_->Destroy(body.entity);
    fading_.clear();
}

void SceneRuntime::Animate(umbra::world::Entity entity, const std::string &trigger)
{
    if (umbra::core::Animator *animator = world_->GetAnimator(entity))
        animator->Trigger(trigger);
}

void SceneRuntime::RefreshWatchList()
{
    owners_ = models_.WatchedFiles();
    owners_[assetRoot_ / scenePath_] = "";
    for (const auto &[path, graph] : graphs_)
        owners_[assetRoot_ / path] = path;
    for (const auto &[path, effect] : effects_)
        owners_[assetRoot_ / path] = path;
    std::set<std::filesystem::path> files;
    for (const auto &[file, owner] : owners_)
        files.insert(file);
    watcher_.SetFiles(files);
}

// ---------------- Player spot and dungeon enemies ----------------

umbra::gameplay::SpawnPoint SceneRuntime::GetPlayerSpot() const
{
    if (!player_.IsSpawned())
        return {};
    return {player_.GetFeet(*physics_), player_.GetFacingYaw()};
}

void SceneRuntime::SpawnRoamers()
{
    // 1. Collect the spawners first (creating entities while iterating the world is unsafe).
    struct Found final
    {
        umbra::world::Spawner spawner;
        glm::vec3 home;
    };
    std::vector<Found> found;
    world_->ForEach([&](umbra::world::Entity entity) {
        if (const umbra::world::Spawner *spawner = world_->GetSpawner(entity))
            found.push_back({*spawner, glm::vec3{(*world_->GetWorldMatrix(entity))[3]}});
    });

    // 2. `count` roamers per spawner: a random actor from the pool at a random point near home.
    for (const Found &f : found)
    {
        for (int i = 0; i < f.spawner.count && !f.spawner.pool.empty(); ++i)
        {
            std::uniform_real_distribution<float> unit(0.0F, 1.0F);
            const std::string &actor = f.spawner.pool[static_cast<std::size_t>(unit(roamerRandom_) * 0.999F * f.spawner.pool.size())];
            const float angle = unit(roamerRandom_) * glm::two_pi<float>();
            const float distance = unit(roamerRandom_) * f.spawner.radius * 0.5F;
            const glm::vec3 feet = f.home + glm::vec3{std::cos(angle) * distance, 0.0F, std::sin(angle) * distance};
            const std::size_t index = AddRoamer(actor, feet, f.home, f.spawner.radius, f.spawner.chase);
            roamers_[index].pool = f.spawner.pool;
            roamers_[index].group = f.spawner.group;
        }
    }
}

std::size_t SceneRuntime::AddRoamer(const std::string &actor, glm::vec3 feet, glm::vec3 home, float homeRadius, bool chase)
{
    // 1. The physics character.
    RoamerSlot slot{umbra::gameplay::Roamer(actor, home, homeRadius, chase), {}, {}, 1};
    slot.roamer.Spawn(*physics_, feet);

    // 2. Its look: from the game (RPG content) or the tinted placeholder cube.
    RoamerLook look;
    look.color = RoamerColor(actor);
    if (roamerLook_)
        if (const std::optional<RoamerLook> chosen = roamerLook_(actor))
            look = *chosen;
    slot.scale = look.scale;
    // The model's lowest point goes on the ground: lift = -(lowest y) * scale.
    slot.lift = look.scale.y;
    if (const LoadedModel *loaded = models_.Load(look.model); loaded && look.mesh < loaded->model->meshes.size())
    {
        // A rigged model is measured as drawn at rest (its vertices are in bind space, S8).
        const umbra::core::MeshData &mesh = loaded->model->meshes[look.mesh].data;
        float lowest = 0.0F, highest = 0.0F;
        const auto measure = [&](float y) { lowest = std::min(lowest, y), highest = std::max(highest, y); };
        if (loaded->model->skeleton)
            for (const glm::vec3 &p : umbra::core::RestPositions(*loaded->model->skeleton, mesh))
                measure(p.y);
        else
            for (const umbra::core::Vertex &v : mesh.vertices)
                measure(v.position.y);
        slot.lift = -lowest * look.scale.y;
        slot.height = (highest - lowest) * look.scale.y;
    }

    // 3. A visual entity with a unique id; Rebind gives it its mesh.
    static int counter = 0;
    slot.visual = world_->Create("roamer_" + std::to_string(++counter), actor);
    umbra::world::Renderable renderable;
    renderable.model = look.model;
    renderable.mesh = look.mesh;
    renderable.color = look.color;
    world_->SetRenderable(slot.visual, renderable);
    if (!look.graph.empty())
        world_->SetAnimationGraph(slot.visual, look.graph);
    roamers_.push_back(std::move(slot));
    SyncRoamerVisuals();
    Rebind();
    return roamers_.size() - 1;
}

void SceneRuntime::RemoveRoamer(std::size_t index, bool animate)
{
    if (index >= roamers_.size() || !roamers_[index].alive)
        return;
    RoamerSlot &slot = roamers_[index];
    slot.roamer.Despawn(*physics_);
    slot.alive = false;
    // A rigged body falls (its "dead" state) and stays a moment; anything else goes at once.
    constexpr float bodySeconds = 2.5F;
    if (umbra::core::Animator *animator = animate ? world_->GetAnimator(slot.visual) : nullptr)
    {
        animator->SetFlag("dead", true);
        fading_.push_back({slot.visual, bodySeconds});
    }
    else
        world_->Destroy(slot.visual);
}

glm::vec3 SceneRuntime::RoamerFeet(std::size_t index) const
{
    return roamers_[index].roamer.GetFeet(*physics_);
}

std::optional<std::size_t> SceneRuntime::TouchingRoamer() const
{
    if (!player_.IsSpawned())
        return std::nullopt;
    // Touching: capsules (0.35 + 0.35 m) plus a small margin apart, at about the same height.
    const glm::vec3 player = player_.GetFeet(*physics_);
    for (std::size_t i = 0; i < roamers_.size(); ++i)
    {
        if (!roamers_[i].alive || roamers_[i].peaceful)
            continue;
        const glm::vec3 feet = RoamerFeet(i);
        if (glm::length(glm::vec2{feet.x - player.x, feet.z - player.z}) < 1.0F && std::abs(feet.y - player.y) < 1.0F)
            return i;
    }
    return std::nullopt;
}

void SceneRuntime::UpdateRoamers(float step)
{
    const glm::vec3 player = player_.IsSpawned() ? player_.GetFeet(*physics_) : glm::vec3{0.0F};
    for (RoamerSlot &slot : roamers_)
    {
        if (!slot.alive)
            continue;
        // A fight's order wins; otherwise wander/chase while active; otherwise stand (gravity still applies).
        const glm::vec3 before = slot.roamer.GetFeet(*physics_);
        if (slot.order)
            slot.roamer.Walk(*physics_, *slot.order, step);
        else if (roamersActive_)
            slot.roamer.Update(*physics_, player, step, roamerRandom_);
        else
            slot.roamer.Walk(*physics_, glm::vec3{0.0F}, step);
        // Face the way it moved (yaw 0 faces -z, like the player).
        const glm::vec3 moved = slot.roamer.GetFeet(*physics_) - before;
        slot.speed = glm::length(glm::vec2{moved.x, moved.z}) / step;
        if (glm::length(glm::vec2{moved.x, moved.z}) > 1e-4F)
            slot.yaw = std::atan2(-moved.x, -moved.z);
    }
}

void SceneRuntime::SyncRoamerVisuals()
{
    // The model stands on the roamer's feet, turned to its facing.
    for (std::size_t i = 0; i < roamers_.size(); ++i)
    {
        if (!roamers_[i].alive)
            continue;
        umbra::world::Transform transform;
        transform.position = RoamerFeet(i) + glm::vec3{0.0F, roamers_[i].lift, 0.0F};
        transform.rotation = glm::angleAxis(roamers_[i].yaw, glm::vec3{0.0F, 1.0F, 0.0F});
        transform.scale = roamers_[i].scale;
        world_->SetLocalTransform(roamers_[i].visual, transform);
    }
    world_->UpdateTransforms();
}

// ---------------- Battle arena and hit feedback ----------------

bool SceneRuntime::EnterArena(const std::string &arenaScene)
{
    if (kept_)
        return false;
    std::unique_ptr<umbra::world::World> fresh = LoadFresh(arenaScene);
    if (!fresh)
        return false;
    // 1. Keep the current area as it is. The player's character leaves its physics world,
    //    so it doesn't stand there as an invisible obstacle when we come back.
    ClearFading(); // bodies don't wait in the kept area
    auto kept = std::make_unique<KeptArea>();
    kept->player = GetPlayerSpot();
    player_.Despawn(*physics_);
    kept->world = std::move(world_);
    kept->physics = std::move(physics_);
    kept->roamers = std::move(roamers_);
    kept->components = std::move(components_);
    kept->scenePath = scenePath_;
    kept_ = std::move(kept);
    // 2. The arena becomes the live world; the player starts at its 'player' marker.
    scenePath_ = arenaScene;
    Install(std::move(fresh), std::nullopt);
    RefreshWatchList();
    return true;
}

bool SceneRuntime::LeaveArena()
{
    if (!kept_)
        return false;
    // 1. The kept area becomes live again (the arena and its physics are destroyed here).
    fading_.clear();
    particles_.Clear();
    attachedEffects_.clear();
    clipEvents_.clear();
    world_ = std::move(kept_->world);
    physics_ = std::move(kept_->physics);
    roamers_ = std::move(kept_->roamers);
    components_ = std::move(kept_->components);
    scenePath_ = kept_->scenePath;
    const umbra::gameplay::SpawnPoint spot = kept_->player;
    kept_.reset();
    // 2. Fresh per-world state, the player back where they were.
    flashes_.clear();
    accumulator_ = 0.0;
    player_.Reset();
    stride_.Reset();
    pendingJump_ = false;
    portalArmed_ = false;
    player_.Spawn(*physics_, spot.feet, spot.yaw);
    ++generation_;
    Rebind(); // models may have been reloaded while away
    RefreshWatchList();
    return true;
}

std::pair<std::string, float> SceneRuntime::Music() const
{
    // The first entity with a "music" component decides (fields as JSON text, Components.md).
    std::pair<std::string, float> music{"", 0.0F};
    world_->ForEach([&](umbra::world::Entity entity) {
        if (const auto *customs = world_->GetCustoms(entity); customs && music.first.empty())
            for (const umbra::world::CustomComponent &custom : *customs)
                if (custom.type == "music")
                {
                    const nlohmann::json data = nlohmann::json::parse(custom.data, nullptr, false);
                    if (data.is_object())
                        music = {data.value("track", ""), data.value("volume", 0.7F)};
                }
    });
    return music;
}

void SceneRuntime::Flash(umbra::world::Entity entity)
{
    constexpr float seconds = 0.2F;
    umbra::world::Renderable *look = world_->GetRenderable(entity);
    if (look == nullptr)
        return;
    for (FlashState &flash : flashes_)
        if (flash.entity == entity)
        {
            flash.secondsLeft = seconds; // already red: just keep it red longer
            return;
        }
    flashes_.push_back({entity, look->color, seconds});
    look->color = {1.0F, 0.15F, 0.1F, 1.0F};
}

void SceneRuntime::UpdateFlashes(float seconds)
{
    // Count down; when done put the original color back (if the entity still exists).
    for (FlashState &flash : flashes_)
    {
        flash.secondsLeft -= seconds;
        if (flash.secondsLeft <= 0.0F)
            if (umbra::world::Renderable *look = world_->GetRenderable(flash.entity))
                look->color = flash.color;
    }
    std::erase_if(flashes_, [](const FlashState &flash) { return flash.secondsLeft <= 0.0F; });
}
