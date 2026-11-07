#pragma once

#include <Diagnostic.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <vector>

namespace umbra::authoring
{
    enum class FileKind
    {
        Unknown,
        Scene, // "format": "umbra.scene"
        Rpg,   // "format": "umbra.rpg"
        Save   // "format": "umbra.save"
    };

    // Reads the file's "format" field.
    [[nodiscard]] FileKind DetectKind(const std::filesystem::path &file);
    [[nodiscard]] const char *ToString(FileKind kind);

    struct ValidateOptions final
    {
        std::filesystem::path assetRoot; // scenes: referenced files must exist here
        std::filesystem::path content;   // saves: the RPG content they must match
    };

    // The same checks the game runs when it loads the file. Scenes also import every model they
    // draw. Appends diagnostics; true if there are no errors.
    bool ValidateFile(const std::filesystem::path &file, const ValidateOptions &options, std::vector<core::Diagnostic> &diagnostics);

    // Diagnostics as JSON, the shape every Umbra tool prints with --json (see Authoring.md).
    [[nodiscard]] nlohmann::json ToJson(const std::vector<core::Diagnostic> &diagnostics);
}
