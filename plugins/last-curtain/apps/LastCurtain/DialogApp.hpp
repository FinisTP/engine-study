#pragma once

#include <AssetFolders.hpp>
#include <Campaign.hpp>

#include <filesystem>
#include <functional>

namespace umbra::core
{
    struct DrawList;
}

struct GameContext;

// What a scene is played over when it has no backdrop: the place it happens in (the board, the café),
// drawn each frame with the camera on whoever speaks (their shown name; "" for the narrator).
using SceneBehind = std::function<void(umbra::core::DrawList &out, float dt, const std::string &speaker)>;

// Plays one scene (campaign/scenes/<id>.json) on the dialog screen over a backdrop, or over `behind`.
// Returns 1 when the scene ends, 0 when the window closes. Facts chosen go into facts.
int RunScene(GameContext &game, const umbra::assets::AssetFolders &assets, const std::string &scene, const std::string &backdrop,
             const umbra::campaign::Speaker &player, umbra::campaign::Facts &facts, const std::filesystem::path &capture = {}, int frames = 0,
             const SceneBehind &behind = {});
