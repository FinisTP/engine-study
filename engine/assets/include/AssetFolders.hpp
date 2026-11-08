#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace umbra::assets
{
    // Where asset paths ("audio/jump.wav") are found (docs/adr/0005-plugins.md): the project's
    // assets folder first, then each enabled plugin's assets folder. A project file with the same
    // path overrides the plugin's. New files are always written into the project.
    //
    // `folders / "x"` returns the first existing file, or the project path when none has it, so
    // code written for one folder keeps working (the same as a single std::filesystem::path).
    class AssetFolders final
    {
    public:
        AssetFolders() = default;
        AssetFolders(std::filesystem::path project) : folders_{std::move(project)} {} // NOLINT: implicit on purpose
        AssetFolders(const char *project) : AssetFolders(std::filesystem::path(project)) {}
        AssetFolders(const std::string &project) : AssetFolders(std::filesystem::path(project)) {}

        // A plugin's assets: read-only, searched after the project and earlier plugins.
        void AddPlugin(std::filesystem::path folder);
        void AddPlugins(const std::string &folders); // "a|b|c", as the build passes them

        [[nodiscard]] std::filesystem::path Find(const std::filesystem::path &relative) const;
        // The folder that holds `relative` (the project when none does).
        [[nodiscard]] const std::filesystem::path &RootOf(const std::filesystem::path &relative) const;
        [[nodiscard]] std::filesystem::path operator/(const std::filesystem::path &relative) const { return Find(relative); }

        [[nodiscard]] const std::filesystem::path &Project() const { return folders_.empty() ? empty_ : folders_.front(); }
        [[nodiscard]] const std::vector<std::filesystem::path> &All() const { return folders_; } // project first
        [[nodiscard]] bool empty() const { return folders_.empty() || folders_.front().empty(); }
        [[nodiscard]] std::string string() const { return Project().string(); }

        // Every file once, as forward-slash asset paths; a project file hides a plugin's.
        [[nodiscard]] std::vector<std::string> ListFiles() const;

    private:
        std::vector<std::filesystem::path> folders_;
        static inline const std::filesystem::path empty_{};
    };
}
