#include <FileWatcher.hpp>

namespace umbra::assets
{
    FileWatcher::Stamp FileWatcher::Read(const std::filesystem::path &path)
    {
        std::error_code ec;
        Stamp stamp;
        stamp.time = std::filesystem::last_write_time(path, ec);
        if (ec)
            return {};
        stamp.size = std::filesystem::file_size(path, ec);
        stamp.exists = !ec;
        return stamp;
    }

    void FileWatcher::SetFiles(const std::set<std::filesystem::path> &files)
    {
        // Keep the state of files we already watch; start new ones from "now, unchanged".
        std::map<std::filesystem::path, Entry> next;
        for (const auto &path : files)
        {
            if (const auto it = files_.find(path); it != files_.end())
                next.emplace(path, it->second);
            else
            {
                const Stamp stamp = Read(path);
                next.emplace(path, Entry{stamp, stamp, Clock::time_point{}});
            }
        }
        files_ = std::move(next);
    }

    std::vector<std::filesystem::path> FileWatcher::Poll(Clock::time_point now)
    {
        std::vector<std::filesystem::path> changed;
        for (auto &[path, entry] : files_)
        {
            // 1. Any difference from the last poll restarts the settle timer.
            const Stamp current = Read(path);
            if (current != entry.seen)
            {
                entry.seen = current;
                entry.since = now;
                continue;
            }
            // 2. Stable for long enough and different from what was reported: report it.
            if (entry.seen != entry.reported && now - entry.since >= settle_)
            {
                entry.reported = entry.seen;
                changed.push_back(path);
            }
        }
        return changed;
    }
}
