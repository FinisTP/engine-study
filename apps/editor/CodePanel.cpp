#include "Editor.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>

// Code window (docs/adr/0004-project-layers.md, 0005-plugins.md): the C++ files of the engine, each
// enabled plugin and the project; a click opens one
// in Visual Studio 2022, "Open project" opens the source folder (CMake mode, CMakePresets.json),
// and "New component" writes a project component (schema + C++) from a template.

namespace
{
    std::filesystem::path SourceRoot() { return std::filesystem::path(UMBRA_SOURCE_DIRECTORY); }
    std::filesystem::path ProjectRoot() { return std::filesystem::path(UMBRA_PROJECT_DIRECTORY); }

#ifdef _WIN32
    // Visual Studio's devenv.exe: asked from vswhere (installed with every VS 2017+), else on PATH.
    std::string FindDevenv()
    {
        static std::string found;
        if (!found.empty())
            return found;
        const char *programs = SDL_getenv("ProgramFiles(x86)");
        const std::string vswhere = std::string(programs ? programs : "C:\\Program Files (x86)") + "\\Microsoft Visual Studio\\Installer\\vswhere.exe";
        const char *arguments[] = {vswhere.c_str(), "-latest", "-property", "productPath", nullptr};
        if (SDL_Process *process = SDL_CreateProcess(arguments, true))
        {
            size_t size = 0;
            int exitCode = 1;
            if (char *output = static_cast<char *>(SDL_ReadProcess(process, &size, &exitCode)))
            {
                found.assign(output, size);
                SDL_free(output);
                while (!found.empty() && std::isspace(static_cast<unsigned char>(found.back())))
                    found.pop_back();
            }
            SDL_DestroyProcess(process);
        }
        if (found.empty())
            found = "devenv";
        return found;
    }

    // Starts the IDE without waiting for it. Returns false if it couldn't be started.
    bool Launch(const std::vector<std::string> &args)
    {
        std::vector<const char *> argv;
        for (const std::string &arg : args)
            argv.push_back(arg.c_str());
        argv.push_back(nullptr);
        SDL_Process *process = SDL_CreateProcess(argv.data(), false);
        if (process == nullptr)
            return false;
        SDL_DestroyProcess(process); // only our handle; the IDE keeps running
        return true;
    }
#endif

    std::string Utf8(const std::filesystem::path &path)
    {
        const std::u8string text = path.generic_u8string();
        return {text.begin(), text.end()};
    }

    void WriteNew(const std::filesystem::path &file, const std::string &text)
    {
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << text;
    }
}

void Editor::ScanCode()
{
    // Every .cpp/.hpp of each layer, grouped by its first folder: the engine, each enabled plugin
    // ("name=folder|..." from the build), then the project's own engine and game code.
    code_.clear();
    std::vector<std::pair<std::string, std::filesystem::path>> layers{{"Engine", SourceRoot() / "engine"}};
    const std::string plugins = UMBRA_PLUGIN_SOURCE_DIRS;
    for (std::size_t start = 0; start < plugins.size();)
    {
        const std::size_t end = std::min(plugins.find('|', start), plugins.size());
        const std::string entry = plugins.substr(start, end - start);
        if (const std::size_t eq = entry.find('='); eq != std::string::npos)
            layers.push_back({"Plugin: " + entry.substr(0, eq), std::filesystem::path(std::u8string(entry.begin() + static_cast<std::ptrdiff_t>(eq) + 1, entry.end()))});
        start = end + 1;
    }
    layers.push_back({"Project engine", ProjectRoot() / "engine"});
    layers.push_back({"Project game", ProjectRoot() / "game"});
    for (const auto &[name, folder] : layers)
    {
        CodeLayer layer{name, folder, {}};
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(folder, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
        {
            const std::string extension = it->path().extension().string();
            if (it->is_regular_file() && (extension == ".cpp" || extension == ".hpp" || extension == ".h"))
                layer.files.push_back(Utf8(std::filesystem::relative(it->path(), folder)));
        }
        std::sort(layer.files.begin(), layer.files.end());
        code_.push_back(std::move(layer));
    }
}

void Editor::OpenCode(const std::filesystem::path &file)
{
    // Visual Studio: "/Edit" opens the file in the running instance (or starts one).
    // Elsewhere: the system's default program for the file.
    bool started = false;
#ifdef _WIN32
    started = file.empty() ? Launch({FindDevenv(), Utf8(SourceRoot())}) : Launch({FindDevenv(), "/Edit", Utf8(file)});
#else
    started = SDL_OpenURL(("file://" + Utf8(file.empty() ? SourceRoot() : file)).c_str());
#endif
    message_ = started ? "Opening " + Utf8(file.empty() ? SourceRoot() : file.filename()) + " in Visual Studio"
                       : "Cannot start Visual Studio 2022 (devenv). Is it installed?";
    messageSeconds_ = 4.0F;
}

void Editor::NewComponent(const std::string &rawName)
{
    // "Door Lock" -> type "door_lock", class "DoorLock".
    std::string type, className;
    bool upper = true;
    for (const char ch : rawName)
    {
        if (std::isalnum(static_cast<unsigned char>(ch)))
        {
            type += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            className += upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(ch))) : ch;
            upper = false;
        }
        else if (!type.empty() && type.back() != '_')
            type += '_', upper = true;
    }
    while (!type.empty() && type.back() == '_')
        type.pop_back();
    const std::filesystem::path cpp = ProjectRoot() / "game" / (className + ".cpp");
    const std::filesystem::path schema = assetRoot_.Project() / "components" / (type + ".component.json");
    if (type.empty() || !std::isalpha(static_cast<unsigned char>(className[0])) || std::filesystem::exists(cpp) || std::filesystem::exists(schema))
    {
        message_ = "Choose a new name that starts with a letter (" + className + ".cpp or " + type + ".component.json exists?)";
        messageSeconds_ = 5.0F;
        return;
    }
    WriteNew(schema, "{\n  \"format\": \"umbra.component\",\n  \"version\": 1,\n  \"type\": \"" + type + "\",\n  \"label\": \"" + rawName +
                         "\",\n  \"description\": \"\",\n  \"fields\": {\n    \"speed\": { \"kind\": \"number\", \"label\": \"Speed\", \"default\": 1 }\n  }\n}\n");
    WriteNew(cpp, "// Project component \"" + type + "\": fields in assets/components/" + type + ".component.json.\n"
                  "#include <GameComponents.hpp>\n\n"
                  "namespace\n{\n"
                  "    class " + className + " final : public umbra::game::GameComponent\n    {\n    public:\n"
                  "        explicit " + className + "(const nlohmann::json &fields) : speed_(fields.value(\"speed\", 1.0F)) {}\n\n"
                  "        void Start(umbra::game::ComponentContext &context) override\n        {\n            // Once, when the scene loads.\n            (void)context;\n        }\n\n"
                  "        void Update(umbra::game::ComponentContext &context) override\n        {\n"
                  "            // Every frame: context.world, context.entity, context.seconds.\n            (void)context;\n        }\n\n"
                  "    private:\n        float speed_;\n    };\n}\n\n"
                  "UMBRA_COMPONENT(\"" + type + "\", " + className + ");\n");
    ScanAssets();
    ScanCode();
    OpenCode(cpp);
    message_ = "Made component '" + type + "'. Build the game (CMake picks up the new file) and add it to an entity.";
    messageSeconds_ = 6.0F;
}

void Editor::CodePanel()
{
    if (!showCode_)
        return;
    ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Code", &showCode_))
    {
        ImGui::End();
        return;
    }
    if (code_.empty())
        ScanCode();
    if (ImGui::Button("Open project in Visual Studio"))
        OpenCode({});
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        ScanCode();
    static char name[64] = "";
    ImGui::SetNextItemWidth(180.0F);
    ImGui::InputTextWithHint("##component", "e.g. Door Lock", name, sizeof(name));
    ImGui::SameLine();
    if (ImGui::Button("New component") && name[0] != '\0')
    {
        NewComponent(name);
        name[0] = '\0';
    }
    ImGui::TextDisabled("Project: %s. Click a file to open it.", UMBRA_PROJECT_NAME);
    ImGui::Separator();

    // One tree per layer, one node per module folder.
    for (const CodeLayer &layer : code_)
    {
        const std::string header = layer.name + " (" + std::to_string(layer.files.size()) + ")###" + layer.name;
        if (!ImGui::CollapsingHeader(header.c_str(), layer.name.starts_with("Project") ? ImGuiTreeNodeFlags_DefaultOpen : 0))
            continue;
        if (layer.files.empty())
            ImGui::TextDisabled("  (empty) %s", Utf8(std::filesystem::relative(layer.folder, SourceRoot())).c_str());
        std::string openModule;
        bool moduleOpen = false;
        for (const std::string &file : layer.files)
        {
            const std::size_t slash = file.find('/');
            const std::string module = slash == std::string::npos ? "" : file.substr(0, slash);
            if (module != openModule)
            {
                if (moduleOpen)
                    ImGui::TreePop();
                openModule = module;
                moduleOpen = !module.empty() && ImGui::TreeNode((module + "##" + layer.name).c_str());
            }
            if (!module.empty() && !moduleOpen)
                continue;
            const std::string label = (module.empty() ? file : file.substr(slash + 1)) + "##" + layer.name + file;
            if (ImGui::Selectable(label.c_str()))
                OpenCode(layer.folder / file);
        }
        if (moduleOpen)
            ImGui::TreePop();
    }
    ImGui::End();
}
