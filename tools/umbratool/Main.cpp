// UmbraTool: content checks and test runs without a window, GPU or editor.
// Every command prints the same diagnostics the editor and the game use. See README.md.
// The core commands are here; plugins add theirs from their tool/ folder (ToolCommand.hpp):
// fight (turn-combat), policy-check (learned-ai), draft, texture, model (ai-authoring).
//
// Exit code: 0 = ok, 1 = errors found (or the command couldn't run), 2 = bad usage.

#include "ToolCommand.hpp"

#include <Package.hpp>
#include <RpgContent.hpp>
#include <SaveGame.hpp>
#include <SceneFile.hpp>
#include <Validate.hpp>
#include <WebPack.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using umbra::core::Diagnostic;
    using umbra::core::Severity;
    using umbra::tool::Arguments;
    using umbra::tool::Report;
    using umbra::tool::Usage;
    using Json = nlohmann::json;

    int Validate(const Arguments &args)
    {
        if (args.positional.empty())
            return Usage();
        std::vector<Diagnostic> diagnostics;
        for (const std::string &file : args.positional)
            umbra::authoring::ValidateFile(file, {args.Assets(), args.Path("content")}, diagnostics);
        return Report(diagnostics, args.positional.size(), args.json);
    }

    // A short summary of a content file: what is in it, by id.
    int Inspect(const Arguments &args)
    {
        if (args.positional.size() != 1)
            return Usage();
        const std::filesystem::path file = args.positional[0];
        const umbra::authoring::FileKind kind = umbra::authoring::DetectKind(file);
        std::ifstream in(file, std::ios::binary);
        const Json root = Json::parse(in, nullptr, false);
        if (kind == umbra::authoring::FileKind::Unknown || kind == umbra::authoring::FileKind::Save)
        {
            // Saves are checksummed text inside an envelope; validate them instead.
            std::vector<Diagnostic> diagnostics;
            umbra::authoring::ValidateFile(file, {}, diagnostics);
            return Report(diagnostics, 1, args.json);
        }

        // Scene: entity ids with their components. RPG: ids per section.
        Json summary{{"file", file.string()}, {"kind", umbra::authoring::ToString(kind)}};
        if (kind == umbra::authoring::FileKind::Scene)
        {
            summary["entities"] = Json::array();
            for (const Json &entity : root.value("entities", Json::array()))
            {
                Json line{{"id", entity.value("id", "")}};
                for (const char *part : {"model", "instance", "parent", "collider", "interactable", "portal", "spawner"})
                    if (entity.contains(part))
                        line["has"].push_back(part);
                const Json components = entity.value("components", Json::object());
                for (const auto &[type, value] : components.items())
                    line["has"].push_back(type);
                summary["entities"].push_back(line);
            }
        }
        else
        {
            for (const char *section : {"statuses", "skills", "items", "actors", "quests", "conversations"})
            {
                summary[section] = Json::array();
                for (const Json &entry : root.value(section, Json::array()))
                    summary[section].push_back(entry.value("id", ""));
            }
        }
        if (args.json)
        {
            std::cout << summary.dump(2) << "\n";
            return 0;
        }
        std::cout << summary["file"].get<std::string>() << " (" << summary["kind"].get<std::string>() << ")\n";
        for (const auto &[key, value] : summary.items())
        {
            if (!value.is_array())
                continue;
            std::cout << "  " << key << " (" << value.size() << "):";
            for (const Json &entry : value)
                std::cout << " " << (entry.is_object() ? entry["id"].get<std::string>() : entry.get<std::string>());
            std::cout << "\n";
        }
        return 0;
    }

    // Format versions this build reads, and where each format is described.
    int Schema(const Arguments &args)
    {
        const Json formats = Json::array({
            {{"format", "umbra.scene"}, {"version", umbra::scene::sceneFormatVersion}, {"doc", "engine/scene/include/SceneFormat.md"}},
            {{"format", "umbra.rpg"}, {"version", umbra::rpg::contentFormatVersion}, {"doc", "plugins/rpg/modules/rpg/include/Rpg.md, plugins/story/modules/story/include/Story.md"}},
            {{"format", "umbra.save"}, {"version", umbra::game::saveFormatVersion}, {"doc", "plugins/rpg-session/modules/game/include/SaveGame.md"}},
        });
        if (args.json)
            std::cout << Json{{"formats", formats}}.dump(2) << "\n";
        else
            for (const Json &f : formats)
                std::cout << f["format"].get<std::string>() << " v" << f["version"].get<int>() << "  " << f["doc"].get<std::string>() << "\n";
        return 0;
    }

    // Release gate (P9): what a packaged game folder may and must contain (Package.md).
    int PackageCheck(const Arguments &args)
    {
        if (args.positional.size() != 1)
            return Usage();
        std::vector<Diagnostic> diagnostics;
        umbra::authoring::CheckPackage(args.positional[0], args.Path("source"), diagnostics);
        return Report(diagnostics, 1, args.json);
    }

    // Browser build: the page loads assets.bin/assets.json, so edited content needs no rebuild.
    int WebPack(const Arguments &args)
    {
        if (args.positional.size() != 2)
            return Usage();
        std::vector<Diagnostic> diagnostics;
        const std::string version = umbra::authoring::PackWebAssets(Arguments::AssetsOf(args.positional[0]), args.positional[1], diagnostics);
        if (!version.empty())
            std::cout << "assets packed into " << args.positional[1] << " (version " << version << ")\n";
        return Report(diagnostics, 1, args.json);
    }

    // Phones (P11): assets + shaders in game.pack for the app package (apps/umbra/Mobile.md).
    int GamePack(const Arguments &args)
    {
        if (args.positional.size() != 3)
            return Usage();
        std::vector<Diagnostic> diagnostics;
        const std::string hash = umbra::authoring::PackGame(Arguments::AssetsOf(args.positional[0]), args.positional[1], args.positional[2], diagnostics);
        if (!hash.empty())
            std::cout << "game.pack written to " << args.positional[2] << " (version " << hash << ")\n";
        return Report(diagnostics, 1, args.json);
    }
}

UMBRA_TOOL_COMMAND(Validate, "validate", "validate <file>... [--assets <folder>] [--content <rpg file>] [--json]", Validate);
UMBRA_TOOL_COMMAND(Inspect, "inspect", "inspect  <file> [--json]", Inspect);
UMBRA_TOOL_COMMAND(Schema, "schema", "schema   [--json]", Schema);
UMBRA_TOOL_COMMAND(PackageCheck, "package-check", "package-check <package folder> [--source <project folder>] [--json]", PackageCheck);
UMBRA_TOOL_COMMAND(WebPack, "web-pack", "web-pack <project assets folder> <web build folder>   (the plugins' assets go in too)", WebPack);

int main(int argc, char **argv)
{
    // 1. The command, then its words and "--key value" options (--json and --apply have no value).
    if (argc < 2)
        return Usage();
    const std::string name = argv[1];
    Arguments args;
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--json")
            args.json = true;
        else if (arg == "--apply")
            args.apply = true;
        else if (arg.starts_with("--") && i + 1 < argc)
            args.options[arg.substr(2)] = argv[++i];
        else if (!arg.empty() && arg[0] != '-')
            args.positional.push_back(arg);
        else
            return Usage();
    }

    // 2. Run it (a bad number in an option is bad usage).
    const auto &commands = umbra::tool::Commands();
    const auto command = std::find_if(commands.begin(), commands.end(), [&](const umbra::tool::Command &c) { return c.name == name; });
    if (command == commands.end())
        return Usage();
    try
    {
        return command->run(args);
    }
    catch (const std::invalid_argument &)
    {
        return Usage();
    }
    catch (const std::out_of_range &)
    {
        return Usage();
    }
}
UMBRA_TOOL_COMMAND(GamePack, "game-pack", "game-pack <project assets folder> <shaders folder> <output folder>   (for the Android app)", GamePack);
