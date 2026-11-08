#pragma once

#include <Diagnostic.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace umbra::authoring
{
    // Browser build (P10, WebPack.md): packs a project's assets folder into assets.bin + assets.json
    // next to the web page, and puts the assets' hash into sw.js so the offline cache updates.
    // AI working files are left out. Returns the assets hash ("" on failure).
    std::string PackWebAssets(const std::filesystem::path &assets, const std::filesystem::path &webFolder, std::vector<core::Diagnostic> &diagnostics);
}
