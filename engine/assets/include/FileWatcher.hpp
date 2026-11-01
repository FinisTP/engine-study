#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <vector>

namespace umbra::assets
{
    // Polls files for changes. A file is reported once its size and time have changed
    // and then stayed the same for `settle`, so a file still being written (editors often
    // save in several steps) is not picked up half-way. See Reload.md.
    class FileWatcher final
    {
    public:
        using Clock = std::chrono::steady_clock;

        explicit FileWatcher(std::chrono::milliseconds settle = std::chrono::milliseconds{250}) : settle_(settle) {}

        // Replaces the watched set. Newly added files start from their current state (no report).
        void SetFiles(const std::set<std::filesystem::path> &files);
        // Returns files that changed and have since been stable for `settle`.
        [[nodiscard]] std::vector<std::filesystem::path> Poll(Clock::time_point now);
        [[nodiscard]] std::size_t Count() const noexcept { return files_.size(); }

    private:
        struct Stamp final
        {
            std::filesystem::file_time_type time{};
            std::uintmax_t size = 0;
            bool exists = false;
            friend bool operator==(const Stamp &, const Stamp &) = default;
        };

        struct Entry final
        {
            Stamp reported;          // state the owner last heard about
            Stamp seen;              // state at the last poll
            Clock::time_point since; // when `seen` was first observed
        };

        static Stamp Read(const std::filesystem::path &path);

        std::chrono::milliseconds settle_;
        std::map<std::filesystem::path, Entry> files_;
    };
}
