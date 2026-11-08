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

        // Writes `entries` (path in the pack, file on disk) as <name>.bin + <name>.json, replacing
        // both only at the end; returns the 12-digit hash of names and contents ("" on failure).
        std::string WritePack(const std::vector<std::pair<std::string, std::filesystem::path>> &entries, const std::filesystem::path &folder,
                              const std::string &binName, const std::string &jsonName, const char *format, std::vector<core::Diagnostic> &diagnostics)
        {
            std::error_code ec;
            std::filesystem::create_directories(folder, ec);
            std::ofstream bin(folder / (binName + ".tmp"), std::ios::binary);
            nlohmann::json index{{"format", format}, {"version", 1}, {"files", nlohmann::json::array()}};
            std::uint64_t hash = 14695981039346656037ULL, offset = 0;
            for (const auto &[name, file] : entries)
            {
                std::ifstream in(file, std::ios::binary);
                const std::string bytes{std::istreambuf_iterator<char>(in), {}};
                Mix(hash, name.data(), name.size());
                Mix(hash, bytes.data(), bytes.size());
                bin.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
                index["files"].push_back({{"path", name}, {"offset", offset}, {"size", bytes.size()}});
                offset += bytes.size();
            }
            bin.close();
            if (!bin)
            {
                diagnostics.push_back({core::Severity::Error, "webpack.io", (folder / binName).generic_string(), "", "could not write"});
                return {};
            }
            char hex[17];
            std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(hash));
            index["size"] = offset;
            index["hash"] = std::string(hex).substr(0, 12);
            std::ofstream(folder / (jsonName + ".tmp")) << index.dump(1);
            // Replace both only now, so a reader never sees a half-written pair.
            std::filesystem::rename(folder / (binName + ".tmp"), folder / binName, ec);
            std::filesystem::rename(folder / (jsonName + ".tmp"), folder / jsonName, ec);
            if (ec)
            {
                diagnostics.push_back({core::Severity::Error, "webpack.io", folder.generic_string(), "", "could not replace " + binName + ": " + ec.message()});
                return {};
            }
            return index["hash"];
        }

        // Every asset file of the project and its plugins (a project file hides a plugin's), sorted
        // so the same folders always give the same package; `prefix` goes in front of each path.
        std::vector<std::pair<std::string, std::filesystem::path>> AssetEntries(const assets::AssetFolders &assets, const std::string &prefix)
        {
            std::vector<std::pair<std::string, std::filesystem::path>> entries;
            for (const std::string &file : assets.ListFiles())
            {
                const std::filesystem::path relative(std::u8string(file.begin(), file.end()));
                if (!Skip(relative))
                    entries.emplace_back(prefix + relative.generic_string(), assets / relative);
            }
            std::sort(entries.begin(), entries.end());
            return entries;
        }
    }

    std::string PackWebAssets(const assets::AssetFolders &assets, const std::filesystem::path &webFolder, std::vector<core::Diagnostic> &diagnostics)
    {
        const auto fail = [&](const std::filesystem::path &file, const std::string &message) {
            diagnostics.push_back({core::Severity::Error, "webpack.io", file.generic_string(), "", message});
            return std::string{};
        };
        if (!std::filesystem::is_directory(assets.Project()))
            return fail(assets.Project(), "no assets folder");
        if (!std::filesystem::exists(webFolder / "umbra.js"))
            return fail(webFolder, "no web build here (umbra.js missing)");

        // 1-2. assets.bin = the files back to back; assets.json = where each one is.
        const std::string assetsHash = WritePack(AssetEntries(assets, ""), webFolder, "assets.bin", "assets.json", "umbra.web-assets", diagnostics);
        if (assetsHash.empty())
            return {};

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

    std::string PackGame(const assets::AssetFolders &assets, const std::filesystem::path &shaderFolder, const std::filesystem::path &outFolder,
                         std::vector<core::Diagnostic> &diagnostics)
    {
        // assets/... plus shaders/*.spv: what a phone unpacks next to its program (Mobile.md).
        if (!std::filesystem::is_directory(assets.Project()) || !std::filesystem::exists(shaderFolder / "Scene.vert.spv"))
        {
            diagnostics.push_back({core::Severity::Error, "pack.input", (std::filesystem::exists(shaderFolder / "Scene.vert.spv") ? assets.Project() : shaderFolder).generic_string(), "",
                                   "needs the assets folder and the compiled shaders (Scene.vert.spv ...)"});
            return {};
        }
        auto entries = AssetEntries(assets, "assets/");
        for (const auto &file : std::filesystem::recursive_directory_iterator(shaderFolder)) // with surface/ (CustomShaders.md)
            if (file.path().extension() == ".spv")
                entries.emplace_back("shaders/" + std::filesystem::relative(file.path(), shaderFolder).generic_string(), file.path());
        std::sort(entries.begin(), entries.end());
        return WritePack(entries, outFolder, "game.pack", "game.pack.json", "umbra.game-pack", diagnostics);
    }
}
