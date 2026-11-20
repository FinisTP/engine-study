#pragma once

#include <AssetFolders.hpp>

#include <filesystem>

struct GameContext;
namespace umbra::cafe { struct Career; }

// The café hub (CafePlay.md): walk, run the shop, talk, look at the company. Returns 1 when the
// stage door is taken (on to the next match), 0 when the window closes. The career is saved on the way out.
int RunCafe(GameContext &game, const umbra::assets::AssetFolders &assets, umbra::cafe::Career &career, const std::filesystem::path &careerFile,
            const std::filesystem::path &capture = {}, int frames = 0);
