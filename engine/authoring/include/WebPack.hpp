#pragma once

#include <AssetFolders.hpp>
#include <Diagnostic.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace umbra::authoring
{
    // Browser build (P10, WebPack.md): packs a project's assets (and its plugins') into assets.bin + assets.json
    // next to the web page, and puts the assets' hash into sw.js so the offline cache updates.
    // AI working files are left out. Returns the assets hash ("" on failure).
    std::string PackWebAssets(const assets::AssetFolders &assets, const std::filesystem::path &webFolder, std::vector<core::Diagnostic> &diagnostics);

    // Phones (P11, apps/umbra/Mobile.md): the assets under assets/ and the compiled shaders under
    // shaders/ in game.pack + game.pack.json (same layout as assets.bin), for the app package; the
    // game unpacks it on first start (assets::UnpackGame). Returns the pack's hash ("" on failure).
    std::string PackGame(const assets::AssetFolders &assets, const std::filesystem::path &shaderFolder, const std::filesystem::path &outFolder,
                         std::vector<core::Diagnostic> &diagnostics);
}
