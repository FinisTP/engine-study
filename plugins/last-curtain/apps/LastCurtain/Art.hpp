#pragma once

#include <Night.hpp>
#include <Post.hpp>

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

    // How the night looks at this hour (Art.md "The night's mood"; user 2026-11-15): from 19:00 a
    // creep (colours drain, colder, darker corners, the lamp flickers), at the Turn a snap to blood.
    struct Mood
    {
        float creep = 0.0F;      // 0 at 19:00 .. 1 at midnight
        float blood = 0.0F;      // 0 before the Turn; 0.35 at it .. 1 at dawn: stains on the floors, drips
        float flicker = 0.0F;    // how much the lamp flickers
        bool bloodGrade = false; // the blood colour table (art/ui/blood_lut.png)
        umbra::core::PostSettings post;
    };
    [[nodiscard]] Mood NightMood(const NightDef &def, const NightState &state);
    // How much walls, figures and items glow so they stay readable under the mood (their emissive).
    [[nodiscard]] float Readability(const Mood &mood);

    // Every picture the data needs that is not under `assets` (paths as ArtPath gives them).
    [[nodiscard]] std::vector<std::string> MissingArt(const NightDef &def, const std::filesystem::path &assets);
}
