#pragma once

#include <AssetFolders.hpp>
#include <Campaign.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

struct GameContext;

// One campaign stage (campaign/stages/<id>.json) played in the match screen, its scenes on the dialog
// screen at their moments (Campaign.md "Scenes inside a fight"). The party: the player, Bao, Ba Mai up to
// the stage's cap, at its level. refill picks a skirmish's twist. Returns RunMatch's result.
int RunStage(GameContext &game, const umbra::assets::AssetFolders &assets, const std::string &id, int refill, std::uint64_t seed,
             const umbra::campaign::Speaker &player, const std::filesystem::path &capture, int frames);
