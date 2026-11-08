#include <Package.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <regex>

namespace umbra::authoring
{
    namespace
    {
        using Diagnostics = std::vector<core::Diagnostic>;

        std::string Lower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        std::string ReadAll(const std::filesystem::path &file)
        {
            std::ifstream in(file, std::ios::binary);
            return {std::istreambuf_iterator<char>(in), {}};
        }

        // "vcruntime140*.dll" style matching (only '*').
        bool Matches(const std::string &pattern, const std::string &name)
        {
            const std::size_t star = pattern.find('*');
            if (star == std::string::npos)
                return pattern == name;
            const std::string head = pattern.substr(0, star), tail = pattern.substr(star + 1);
            return name.size() >= head.size() + tail.size() && name.starts_with(head) && name.ends_with(tail);
        }

        // Every string in a JSON file that is an absolute path ("C:\...", "/home/...").
        void FindAbsolutePaths(const nlohmann::json &value, std::vector<std::string> &found)
        {
            if (value.is_string())
            {
                const std::string s = value.get<std::string>();
                const bool drive = s.size() > 2 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':' && (s[2] == '\\' || s[2] == '/');
                if (drive || s.starts_with("/home/") || s.starts_with("/Users/") || s.starts_with("\\\\"))
                    found.push_back(s);
            }
            else if (value.is_structured())
                for (const auto &item : value)
                    FindAbsolutePaths(item, found);
        }
    }

    bool CheckPackage(const std::filesystem::path &folder, const std::filesystem::path &sourceRoot, std::vector<core::Diagnostic> &diagnostics)
    {
        const std::size_t before = diagnostics.size();
        const auto add = [&](core::Severity severity, const std::string &code, const std::filesystem::path &file, const std::string &message) {
            diagnostics.push_back({severity, code, file.generic_string(), "", message});
        };
        const auto error = [&](const std::string &code, const std::filesystem::path &file, const std::string &message) {
            add(core::Severity::Error, code, file, message);
        };
        const auto warning = [&](const std::string &code, const std::filesystem::path &file, const std::string &message) {
            add(core::Severity::Warning, code, file, message);
        };
        if (!std::filesystem::is_directory(folder))
        {
            error("package.missing", folder, "not a folder: build it with scripts\\package.bat");
            return false;
        }

        // 1. The program and the content the game loads at start.
        const std::filesystem::path program = std::filesystem::exists(folder / "Umbra.exe") ? folder / "Umbra.exe" : folder / "Umbra";
        if (!std::filesystem::exists(program))
            error("package.program", folder / "Umbra.exe", "the game program is missing");
        for (const char *required : {"assets/rpg/reference.rpg.json", "assets/scenes/settlement.scene.json", "assets/scenes/dungeon.scene.json",
                                     "assets/scenes/arena.scene.json", "shaders/Scene.vert.spv", "shaders/Scene.frag.spv",
                                     "THIRD_PARTY_NOTICES.md"})
            if (!std::filesystem::exists(folder / required))
                error("package.content", folder / required, "missing from the package");
        // Learned enemy models named by the content (the game falls back to the conventional AI, but the package should ship them).
        const nlohmann::json rpg = nlohmann::json::parse(ReadAll(folder / "assets/rpg/reference.rpg.json"), nullptr, false);
        if (rpg.is_object())
            for (const auto &actor : rpg.value("actors", nlohmann::json::array()))
                if (actor.is_object() && actor.contains("policy") && actor["policy"].is_string() &&
                    !std::filesystem::exists(folder / "assets" / actor["policy"].get<std::string>()))
                    error("package.policy", folder / "assets" / actor["policy"].get<std::string>(), "the accepted enemy policy named by the content is missing");

        // 2. Walk every file once.
        const std::string notices = Lower(ReadAll(folder / "THIRD_PARTY_NOTICES.md"));
        const std::regex secret(R"re((msy_[A-Za-z0-9]{16,}|sk-[A-Za-z0-9_-]{20,}|"(api_?key|apikey|password|secret|token)"\s*:\s*"[^"]{4,}"))re", std::regex::icase);
        std::uintmax_t total = 0;
        std::error_code ec;
        for (const auto &entry : std::filesystem::recursive_directory_iterator(folder, ec))
        {
            if (!entry.is_regular_file())
                continue;
            const std::filesystem::path file = entry.path();
            const std::filesystem::path relative = std::filesystem::relative(file, folder);
            const std::string name = Lower(file.filename().string());
            const std::string extension = Lower(file.extension().string());
            const std::uintmax_t size = entry.file_size(ec);
            total += size;

            // Development programs and files never ship.
            for (const char *banned : {"umbraeditor", "umbratool", "umbrapolicyenv", "umbrascenecheck"})
                if (name.starts_with(banned))
                    error("package.forbidden", relative, "development program in the game package");
            if (name.find("tests") != std::string::npos && (extension == ".exe" || extension == ".dll"))
                error("package.forbidden", relative, "test program in the game package");
            if (extension == ".py" || extension == ".bat" || extension == ".ps1" || name == "cmakecache.txt" || name == "umbra-editor.ini" ||
                name.ends_with(".api.json"))
                error("package.forbidden", relative, "development file in the game package");

            // Every shipped library has a license notice.
            if (extension == ".dll" && relative.parent_path().empty())
            {
                bool listed = false;
                std::smatch match;
                const std::regex listedFile(R"(`([a-z0-9_.*+-]+\.dll)`)");
                for (auto it = std::sregex_iterator(notices.begin(), notices.end(), listedFile); it != std::sregex_iterator(); ++it)
                    listed = listed || Matches((*it)[1].str(), name);
                if (!listed)
                    error("package.license", relative, "no license notice: add the library to THIRD_PARTY_NOTICES.md");
            }

            // Text: no secrets, no absolute paths in content.
            if (extension == ".json" || extension == ".txt" || extension == ".md" || extension == ".ini" || extension == ".cfg" || extension == ".gltf")
            {
                const std::string text = ReadAll(file);
                if (std::regex_search(text, secret))
                    error("package.secret", relative, "looks like a key, token or password: remove it (the value is not shown)");
                if (extension == ".json" || extension == ".gltf")
                {
                    std::vector<std::string> absolute;
                    FindAbsolutePaths(nlohmann::json::parse(text, nullptr, false), absolute);
                    // Provenance files record where things came from; content files must not point outside the game.
                    if (!absolute.empty() && !name.ends_with(".source.json"))
                        error("package.path", relative, "absolute path '" + absolute.front() + "': content must use paths relative to assets/");
                }
                if (name.ends_with(".source.json"))
                {
                    const nlohmann::json provenance = nlohmann::json::parse(text, nullptr, false);
                    for (const auto &model : provenance.is_object() ? provenance.value("models", nlohmann::json::array()) : nlohmann::json::array())
                        if (model.is_object() && !model.value("commercial", true))
                            warning("package.ai-license", relative, "made with " + model.value("file", std::string{"a model"}) + " (" +
                                                                         model.value("license", std::string{"unknown"}) + "): check before selling");
                }
            }

            // Leftovers that only matter while creating.
            if (name.ends_with(".concept.png") || relative.generic_string().find("comfy-export") != std::string::npos)
                warning("package.extra", relative, "AI working file, not used by the game (remove it from assets/)");
            if (size > 64ull * 1024 * 1024)
                warning("package.large", relative, std::to_string(size / (1024 * 1024)) + " MiB");
        }

        // 3. The program must not depend on the developer's folders. Its debug information may name
        //    source files (crash reports); a path to the source assets would mean it reads them.
        if (!sourceRoot.empty() && std::filesystem::exists(program))
        {
            const std::string binary = ReadAll(program);
            std::vector<std::string> roots{sourceRoot.generic_string()};
            if (sourceRoot.string() != roots[0])
                roots.push_back(sourceRoot.string()); // Windows: also with backslashes
            for (std::string root : roots)
            {
                while (!root.empty() && (root.back() == '/' || root.back() == '\\'))
                    root.pop_back();
                if (root.size() < 4)
                    continue;
                std::size_t mentions = 0;
                for (std::size_t at = binary.find(root); at != std::string::npos; at = binary.find(root, at + 1))
                {
                    ++mentions;
                    // "<root>/assets" or "<root>/projects/<name>/assets" (either slash).
                    static const std::regex assetsPath(R"re(^[/\\](projects[/\\][^/\\]+[/\\])?assets([^A-Za-z0-9_]|$))re");
                    const std::string after = binary.substr(at + root.size(), 80);
                    if (std::regex_search(after, assetsPath))
                    {
                        error("package.buildpath", program.filename(), "the program contains the source assets path " + root + "/.../assets: it must find assets at run time");
                        break;
                    }
                }
                if (mentions > 0)
                    warning("package.sourcenames", program.filename(), std::to_string(mentions) + " source file names inside (crash report locations); harmless, but they show " + root);
            }
        }
        diagnostics.push_back({core::Severity::Warning, "package.size", folder.generic_string(), "",
                               "package is " + std::to_string(total / (1024 * 1024)) + " MiB"}); // informational
        return !core::HasErrors(std::vector<core::Diagnostic>(diagnostics.begin() + static_cast<std::ptrdiff_t>(before), diagnostics.end()));
    }
}
