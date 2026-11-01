#pragma once

#include "ModelLibrary.hpp"

#include <FileWatcher.hpp>
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

    [[nodiscard]] umbra::world::World &GetWorld() noexcept { return *world_; }

private:
    void Rebind();
    void RefreshWatchList();

    ModelLibrary &models_;
    std::filesystem::path assetRoot_;
    std::string scenePath_;
    std::unique_ptr<umbra::world::World> world_;
    umbra::assets::FileWatcher watcher_;
    std::map<std::filesystem::path, std::string> owners_; // watched file -> model path ("" = the scene file)
    umbra::assets::FileWatcher::Clock::time_point nextPoll_{};
};
