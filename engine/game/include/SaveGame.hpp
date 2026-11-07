#pragma once

#include <QuestLog.hpp>

#include <glm/vec3.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace umbra::game
{
    enum class CombatMode
    {
        Turn,    // default
        Realtime
    };

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
