#pragma once

#include <Campaign.hpp>

#include <nlohmann/json.hpp>

#include <functional>

// The dialog screen's data (ui/dialog.ui.json) from a playing scene: four portrait places, the name
// tag, the text, the choices. exists(path) says whether a picture is in the assets.
nlohmann::json DialogData(const umbra::campaign::ScenePlayer &p, const umbra::campaign::Cast &cast, const umbra::campaign::Speaker &player,
                          const std::function<bool(const std::string &)> &exists);
