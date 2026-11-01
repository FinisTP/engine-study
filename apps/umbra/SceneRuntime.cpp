#include "SceneRuntime.hpp"

#include <Log.hpp>
#include <ModelInstance.hpp>
#include <SceneFile.hpp>

#include <set>

SceneRuntime::SceneRuntime(ModelLibrary &models, std::filesystem::path assetRoot, std::string scenePath)
    : models_(models), assetRoot_(std::move(assetRoot)), scenePath_(std::move(scenePath)),
      world_(std::make_unique<umbra::world::World>())
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
    if (loaded)
    {
        world_ = std::move(fresh);
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
