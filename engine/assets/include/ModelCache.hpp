#pragma once

#include <Model.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace umbra::assets
{
    // Bump modelFormatVersion when the .umodel layout changes, importerVersion when
    // the importer output changes. Either one makes every cached file rebuild.
    inline constexpr std::uint32_t modelFormatVersion = 1;
    inline constexpr std::uint32_t importerVersion = 1;

    struct BuildInfo final
    {
        std::uint64_t sourceHash = 0;          // see HashSource
        std::vector<std::string> dependencies; // relative to the source file's folder
    };

    // Hash of both versions, the source bytes and every dependency's bytes.
    // Returns 0 if any file cannot be read.
    [[nodiscard]] std::uint64_t HashSource(const std::filesystem::path &sourceFile, const std::vector<std::string> &dependencies);

    // Writes atomically (temp file + rename): readers see the old file or the new one, never half.
    [[nodiscard]] bool WriteModelFile(const std::filesystem::path &path, const Model &model, const BuildInfo &info, std::string &error);
    // Fail on missing, truncated, corrupt or wrong-version files.
    [[nodiscard]] bool ReadModelFile(const std::filesystem::path &path, Model &model, BuildInfo &info, std::string &error);
    [[nodiscard]] bool ReadModelInfo(const std::filesystem::path &path, BuildInfo &info, std::string &error);

    enum class BuildStatus
    {
        Built,
        UpToDate,
        Failed
    };

    struct BuildResult final
    {
        BuildStatus status = BuildStatus::Failed;
        std::filesystem::path output;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    // Builds assetRoot/source into cacheRoot/<source>.umodel unless that file is already up to date.
    // On failure the previous cached file (if any) is left untouched.
    [[nodiscard]] BuildResult BuildModel(const std::filesystem::path &assetRoot, const std::filesystem::path &source,
                                         const std::filesystem::path &cacheRoot);

    // Path BuildModel writes to.
    [[nodiscard]] std::filesystem::path CachedModelPath(const std::filesystem::path &source, const std::filesystem::path &cacheRoot);

    // True if `path` is relative and has no ".." parts (asset references must stay inside the asset root).
    [[nodiscard]] bool IsSafeAssetPath(const std::filesystem::path &path);
}
