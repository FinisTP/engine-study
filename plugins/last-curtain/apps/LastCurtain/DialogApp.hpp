#pragma once

#include <AssetFolders.hpp>
#include <Campaign.hpp>

#include <filesystem>

struct GameContext;

// Plays one scene (campaign/scenes/<id>.json) on the dialog screen over a backdrop. Returns 1 when the
// scene ends, 0 when the window closes. Facts chosen go into facts.
int RunScene(GameContext &game, const umbra::assets::AssetFolders &assets, const std::string &scene, const std::string &backdrop,
             const umbra::campaign::Speaker &player, umbra::campaign::Facts &facts, const std::filesystem::path &capture = {}, int frames = 0);
