#include <GamePack.hpp>

#include <ModelCache.hpp> // IsSafeAssetPath

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>

namespace umbra::assets
{
    std::string PackHash(const std::string &index)
    {
        const nlohmann::json j = nlohmann::json::parse(index, nullptr, false);
        return j.is_object() && j.value("format", "") == "umbra.game-pack" ? j.value("hash", "") : "";
    }

    std::string InstalledPackHash(const std::filesystem::path &target)
    {
        std::ifstream in(target / ".pack-hash");
        return {std::istreambuf_iterator<char>(in), {}};
    }

    bool UnpackGame(const std::string &index, std::span<const std::byte> bin, const std::filesystem::path &target, std::string &error)
    {
        const nlohmann::json j = nlohmann::json::parse(index, nullptr, false);
        if (!j.is_object() || j.value("format", "") != "umbra.game-pack" || j.value("version", 0) != 1 || !j.contains("files"))
        {
            error = "not an Umbra game pack index (game.pack.json)";
            return false;
        }
        // 1. The old hash goes first: if writing stops half way, the next start unpacks again.
        std::error_code ec;
        std::filesystem::create_directories(target, ec);
        std::filesystem::remove(target / ".pack-hash", ec);
        // 2. Each file, checked: a safe relative path and a range inside the pack.
        for (const nlohmann::json &file : j["files"])
        {
            const std::string path = file.value("path", "");
            const auto offset = file.value("offset", std::uint64_t{0}), size = file.value("size", std::uint64_t{0});
            if (!IsSafeAssetPath(path) || offset + size > bin.size() || offset + size < offset)
            {
                error = "bad entry in the pack: '" + path + "'";
                return false;
            }
            const std::filesystem::path out = target / std::filesystem::path(std::u8string(path.begin(), path.end()));
            std::filesystem::create_directories(out.parent_path(), ec);
            std::ofstream stream(out, std::ios::binary | std::ios::trunc);
            if (!stream.write(reinterpret_cast<const char *>(bin.data() + offset), static_cast<std::streamsize>(size)))
            {
                error = "could not write " + out.string() + " (storage full?)";
                return false;
            }
        }
        // 3. Done: remember which pack this is.
        std::ofstream(target / ".pack-hash") << j.value("hash", "");
        return true;
    }
}
