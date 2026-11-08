#include <AssetFolders.hpp>

#include <algorithm>
#include <set>

namespace umbra::assets
{
    void AssetFolders::AddPlugin(std::filesystem::path folder)
    {
        if (folders_.empty())
            folders_.emplace_back(); // no project yet: an empty project slot keeps the order
        if (std::find(folders_.begin(), folders_.end(), folder) == folders_.end())
            folders_.push_back(std::move(folder));
    }

    void AssetFolders::AddPlugins(const std::string &folders)
    {
        std::size_t start = 0;
        while (start < folders.size())
        {
            const std::size_t end = std::min(folders.find('|', start), folders.size());
            if (end > start)
                AddPlugin(std::u8string(folders.begin() + static_cast<std::ptrdiff_t>(start), folders.begin() + static_cast<std::ptrdiff_t>(end)));
            start = end + 1;
        }
    }

    const std::filesystem::path &AssetFolders::RootOf(const std::filesystem::path &relative) const
    {
        std::error_code ec;
        for (const std::filesystem::path &folder : folders_)
            if (!folder.empty() && std::filesystem::exists(folder / relative, ec))
                return folder;
        return Project();
    }

    std::filesystem::path AssetFolders::Find(const std::filesystem::path &relative) const
    {
        return RootOf(relative) / relative;
    }

    std::vector<std::string> AssetFolders::ListFiles() const
    {
        std::set<std::string> files;
        for (const std::filesystem::path &folder : folders_)
        {
            std::error_code ec;
            if (folder.empty() || !std::filesystem::is_directory(folder, ec))
                continue;
            for (auto it = std::filesystem::recursive_directory_iterator(folder, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
                if (it->is_regular_file())
                {
                    const std::u8string text = std::filesystem::relative(it->path(), folder).generic_u8string();
                    files.insert(std::string(text.begin(), text.end()));
                }
        }
        return {files.begin(), files.end()};
    }
}
