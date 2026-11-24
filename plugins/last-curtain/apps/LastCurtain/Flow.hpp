#pragma once

#include <AssetFolders.hpp>
#include <Campaign.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <functional>
#include "Analog.hpp"

#include <string>
#include <utility>

struct GameContext;

// One screen until the player picks something (CampaignApp.md): returns {action, arg}; {"quit", ""} when the
// window closes. `refresh` may update the data each frame and return true to keep waiting after an action.
std::pair<std::string, std::string> RunScreen(GameContext &game, const std::string &screen, nlohmann::json data,
                                              const std::function<bool(const std::string &action, const std::string &arg, nlohmann::json &data)> &handle = {},
                                              const std::filesystem::path &capture = {}, int frames = 0,
                                              const std::function<void(nlohmann::json &data)> &each = {}); // each frame, before drawing

// The settings screen (menus.md D) over whatever is showing; each change applies and saves at once. words:
// campaign-words.json. Returns the settings as left.
umbra::night::AnalogScreen RunSettings(GameContext &game, const nlohmann::json &words);

// The campaign from the title to the end of slice 1 (CampaignApp.md). save: the save file.
int RunCampaign(GameContext &game, const umbra::assets::AssetFolders &assets, const std::filesystem::path &save, const std::filesystem::path &capture = {}, int frames = 0);
