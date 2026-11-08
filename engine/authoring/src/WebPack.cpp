#include <WebPack.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <regex>

namespace umbra::authoring
{
    namespace
    {
        // Files the game never loads (AI working files, exported workflows).
        bool Skip(const std::filesystem::path &relative)
        {
            const std::string text = relative.generic_string();
            return text.ends_with(".concept.png") || text.ends_with(".api.json") || text.find("comfy-export") != std::string::npos;
        }

        // FNV-1a, 64 bit: a cheap fingerprint of names and contents (not security).
        void Mix(std::uint64_t &hash, const char *data, std::size_t size)
        {
            for (std::size_t i = 0; i < size; ++i)
                hash = (hash ^ static_cast<unsigned char>(data[i])) * 1099511628211ULL;
        }
    }

    std::string PackWebAssets(const std::filesystem::path &assets, const std::filesystem::path &webFolder, std::vector<core::Diagnostic> &diagnostics)
    {
        const auto fail = [&](const std::filesystem::path &file, const std::string &message) {
            diagnostics.push_back({core::Severity::Error, "webpack.io", file.generic_string(), "", message});
            return std::string{};
        };
        if (!std::filesystem::is_directory(assets))
            return fail(assets, "no assets folder");
        if (!std::filesystem::exists(webFolder / "umbra.js"))
            return fail(webFolder, "no web build here (umbra.js missing)");

        // 1. Every asset file, sorted so the same folder always gives the same package.
        std::vector<std::filesystem::path> files;
        for (const auto &entry : std::filesystem::recursive_directory_iterator(assets))
            if (entry.is_regular_file() && !Skip(std::filesystem::relative(entry.path(), assets)))
                files.push_back(std::filesystem::relative(entry.path(), assets));
        std::sort(files.begin(), files.end());

        // 2. assets.bin = the files back to back; assets.json = where each one is.
        std::ofstream bin(webFolder / "assets.bin.tmp", std::ios::binary);
        nlohmann::json index{{"format", "umbra.web-assets"}, {"version", 1}, {"files", nlohmann::json::array()}};
        std::uint64_t hash = 14695981039346656037ULL, offset = 0;
        for (const auto &relative : files)
        {
            std::ifstream in(assets / relative, std::ios::binary);
            const std::string bytes{std::istreambuf_iterator<char>(in), {}};
            const std::string name = relative.generic_string();
            Mix(hash, name.data(), name.size());
            Mix(hash, bytes.data(), bytes.size());
            bin.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            index["files"].push_back({{"path", name}, {"offset", offset}, {"size", bytes.size()}});
            offset += bytes.size();
        }
        bin.close();
        if (!bin)
            return fail(webFolder / "assets.bin", "could not write");
        char hex[17];
        std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(hash));
        const std::string assetsHash = std::string(hex).substr(0, 12);
        index["size"] = offset;
        index["hash"] = assetsHash;
        std::ofstream(webFolder / "assets.json.tmp") << index.dump(1);
        // Replace both only now, so a page never sees a half-written pair.
        std::error_code ec;
        std::filesystem::rename(webFolder / "assets.bin.tmp", webFolder / "assets.bin", ec);
        std::filesystem::rename(webFolder / "assets.json.tmp", webFolder / "assets.json", ec);
        if (ec)
            return fail(webFolder, "could not replace assets.bin/assets.json: " + ec.message());

        // 3. The offline cache's version = program part + assets part (sw.js, made by stamp.cmake).
        std::ifstream swIn(webFolder / "sw.js");
        std::string sw{std::istreambuf_iterator<char>(swIn), {}};
        swIn.close();
        const std::regex version(R"(const VERSION = '([^'+]*)(\+[^']*)?';)");
        if (!std::regex_search(sw, version))
            return fail(webFolder / "sw.js", "no VERSION line: the web build is too old for web-pack");
        sw = std::regex_replace(sw, version, "const VERSION = '$1+" + assetsHash + "';");
        std::ofstream(webFolder / "sw.js") << sw;
        return assetsHash;
    }
}
