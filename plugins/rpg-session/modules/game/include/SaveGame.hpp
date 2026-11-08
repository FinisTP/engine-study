#pragma once

#include <QuestLog.hpp>

#include <glm/vec3.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace umbra::game
{
    // Save envelope version written by WriteSave ("format": "umbra.save").
    inline constexpr int saveFormatVersion = 1;

    enum class CombatMode
    {
        Turn,    // default
        Realtime
    };

    // The combat modes this project has: each is a plugin (turn-combat, realtime-combat).
#if UMBRA_PLUGIN_TURN_COMBAT
    inline constexpr bool hasTurnMode = true;
#else
    inline constexpr bool hasTurnMode = false;
#endif
#if UMBRA_PLUGIN_REALTIME_COMBAT
    inline constexpr bool hasRealtimeMode = true;
#else
    inline constexpr bool hasRealtimeMode = false;
#endif
    // `wanted` if the project has it, else the mode it has (a save from a two-mode build still loads).
    [[nodiscard]] constexpr CombatMode AvailableMode(CombatMode wanted = CombatMode::Turn)
    {
        if (wanted == CombatMode::Turn ? hasTurnMode : hasRealtimeMode)
            return wanted;
        return hasTurnMode ? CombatMode::Turn : CombatMode::Realtime;
    }

    // Everything a player save holds. Taken only outside combat (a safe boundary), so there are
    // no statuses, cooldowns or pending decisions to store. Format and rules in SaveGame.md.
    struct SaveData final
    {
        std::string scene;       // area asset path
        glm::vec3 feet{0.0F};    // player position
        float yaw = 0.0F;
        rpg::Character hero;
        std::map<std::string, story::QuestProgress> quests;
        CombatMode mode = CombatMode::Turn;
        std::vector<std::string> hotkeys; // real-time keys 1-4: "skill:<id>", "item:<id>" or "" (optional in the file)
    };

    // The two save files (user decision 2026-11-07): save stones write the manual save,
    // area changes write the autosave.
    struct SaveSlots final
    {
        std::filesystem::path directory;
        [[nodiscard]] std::filesystem::path Manual() const { return directory / "manual.sav"; }
        [[nodiscard]] std::filesystem::path Autosave() const { return directory / "autosave.sav"; }
    };

    // Writes to "<file>.tmp", checks it reads back, keeps the previous good file as "<file>.bak",
    // then replaces the file. On any failure the existing save is untouched.
    bool WriteSave(const std::filesystem::path &file, const SaveData &data, std::vector<core::Diagnostic> &diagnostics);

    // Reads and fully validates a save (format, version, checksum, every id against the content).
    // Fills `data` only on success.
    bool ReadSave(const std::filesystem::path &file, const rpg::Content &content, SaveData &data,
                  std::vector<core::Diagnostic> &diagnostics);

    // ReadSave, falling back to "<file>.bak" (with a warning) if the main file is damaged.
    bool ReadSaveOrBackup(const std::filesystem::path &file, const rpg::Content &content, SaveData &data,
                          std::vector<core::Diagnostic> &diagnostics);
}
