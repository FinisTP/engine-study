#pragma once

#include <AssetFolders.hpp>

#include <cstdint>
#include <filesystem>

// LastCurtain's faction match (MatchPlay.md): pick, match, end, pick again. seed 0 = from the clock;
// watch: both sides are bots from the start; capture: save the frame after `frames` and quit.
int RunMatch(const umbra::assets::AssetFolders &assets, std::uint64_t seed, bool watch, const std::filesystem::path &capture, int frames);
