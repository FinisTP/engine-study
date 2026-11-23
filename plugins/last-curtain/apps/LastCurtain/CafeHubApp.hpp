#pragma once

#include <AssetFolders.hpp>
#include <Campaign.hpp>

#include <filesystem>

struct GameContext;
namespace umbra::campaign { struct Save; }

// The campaign cafe on screen (CafeHub.hpp): talk, brew, then leave by the door. Talks and memory drinks
// play their scenes on the dialog screen. Returns 1 at the door, 0 when the window closes.
int RunCafeHub(GameContext &game, const umbra::assets::AssetFolders &assets, umbra::campaign::Save &save, const umbra::campaign::Speaker &player,
               const std::filesystem::path &capture = {}, int frames = 0);
