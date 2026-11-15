#pragma once

#include <Night.hpp>

#include <filesystem>
#include <string>
#include <vector>

// The table's painted pictures (plan 4a, Art.md): named by their data id, so no mapping file.
namespace umbra::night
{
    // "art/<set>/<id>.jpg" (rooms, events, portraits) or ".png" (items, cards, ui).
    [[nodiscard]] std::filesystem::path ArtPath(const std::string &set, const std::string &id);

    // The cells the guests started in tonight: rebuilt from the night's seed, so a save needs nothing new.
    [[nodiscard]] std::vector<int> StartCells(const NightDef &def, const NightState &state);

    // Per cell: the foyer, the under-stage, a start cell or a room named in a goal (a gold frame).
    [[nodiscard]] std::vector<bool> FeaturedCells(const NightDef &def, const NightState &state);
    [[nodiscard]] bool Featured(const NightDef &def, const NightState &state, int cell);

    // Every picture the data needs that is not under `assets` (paths as ArtPath gives them).
    [[nodiscard]] std::vector<std::string> MissingArt(const NightDef &def, const std::filesystem::path &assets);
}
