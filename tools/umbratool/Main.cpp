// UmbraTool: content checks and test runs without a window, GPU or editor.
// Every command prints the same diagnostics the editor and the game use. See README.md.
//
//   UmbraTool validate <file>... [--assets <folder>] [--content <rpg file>] [--json]
//   UmbraTool inspect  <file> [--json]
//   UmbraTool schema   [--json]
//   UmbraTool fight    <rpg file> <hero actor> <enemy actor>... [--json]
//   UmbraTool policy-check <policy file> --content <rpg file> [--json]
//
// Exit code: 0 = ok, 1 = errors found (or the fight couldn't run), 2 = bad usage.

#include <Document.hpp>
#include <PolicyModel.hpp>
#include <RpgContent.hpp>
#include <SaveGame.hpp>
#include <SceneFile.hpp>
#include <TurnEnvironment.hpp>
#include <Validate.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace
{
    using umbra::core::Diagnostic;
    using umbra::core::Severity;
    using Json = nlohmann::json;

    struct Arguments final
    {
        std::vector<std::string> positional;
        std::filesystem::path assets;
        std::filesystem::path content;
        bool json = false;
    };

    int Usage()
    {
        std::cerr << "usage:\n"
                     "  UmbraTool validate <file>... [--assets <folder>] [--content <rpg file>] [--json]\n"
                     "  UmbraTool inspect  <file> [--json]\n"
                     "  UmbraTool schema   [--json]\n"
                     "  UmbraTool fight    <rpg file> <hero actor> <enemy actor>... [--json]\n"
                     "  UmbraTool policy-check <policy file> --content <rpg file> [--json]\n";
        return 2;
    }

    // Prints diagnostics as text lines or JSON; returns the exit code.
    int Report(const std::vector<Diagnostic> &diagnostics, std::size_t files, bool json)
    {
        int errors = 0, warnings = 0;
        for (const Diagnostic &d : diagnostics)
            (d.severity == Severity::Error ? errors : warnings)++;
        if (json)
            std::cout << Json{{"files", files}, {"errors", errors}, {"warnings", warnings}, {"diagnostics", umbra::authoring::ToJson(diagnostics)}}.dump(2) << "\n";
        else
        {
            for (const Diagnostic &d : diagnostics)
                std::cout << umbra::core::Format(d) << "\n";
            std::cout << files << " file(s): " << errors << " error(s), " << warnings << " warning(s)\n";
        }
        return errors > 0 ? 1 : 0;
    }

    int Validate(const Arguments &args)
    {
        if (args.positional.empty())
            return Usage();
        std::vector<Diagnostic> diagnostics;
        for (const std::string &file : args.positional)
            umbra::authoring::ValidateFile(file, {args.assets, args.content}, diagnostics);
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
                for (const char *part : {"model", "instance", "collider", "interactable", "portal", "spawner", "parent"})
                    if (entity.contains(part))
                        line["has"].push_back(part);
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
            {{"format", "umbra.rpg"}, {"version", umbra::rpg::contentFormatVersion}, {"doc", "engine/rpg/include/Rpg.md, engine/story/include/Story.md"}},
            {{"format", "umbra.save"}, {"version", umbra::game::saveFormatVersion}, {"doc", "engine/game/include/SaveGame.md"}},
        });
        if (args.json)
            std::cout << Json{{"formats", formats}}.dump(2) << "\n";
        else
            for (const Json &f : formats)
                std::cout << f["format"].get<std::string>() << " v" << f["version"].get<int>() << "  " << f["doc"].get<std::string>() << "\n";
        return 0;
    }

    // A turn fight with the conventional AI on both sides: a quick balance check for content.
    int Fight(const Arguments &args)
    {
        if (args.positional.size() < 3)
            return Usage();
        umbra::rpg::Content content;
        std::vector<Diagnostic> diagnostics;
        if (!umbra::rpg::LoadContent(args.positional[0], content, diagnostics))
            return Report(diagnostics, 1, args.json);

        // 1. The hero is team 0, every enemy team 1.
        std::vector<umbra::turn::ScenarioSlot> slots;
        for (std::size_t i = 1; i < args.positional.size(); ++i)
            slots.push_back({args.positional[i], args.positional[i] + "_" + std::to_string(i), i == 1 ? 0 : 1});
        umbra::turn::TurnEnvironment fight(content, slots);
        if (!fight.Reset())
        {
            std::cerr << "unknown actor (or its starting inventory doesn't fit)\n";
            return 1;
        }
        // 2. Step until someone wins (or a generous limit).
        umbra::turn::StepResult step;
        while (!step.done && fight.Steps() < 10000)
            step = fight.Step(umbra::turn::ChooseTurnAction(content, *fight.Encounter()));

        // 3. Result.
        Json result{{"finished", step.done}, {"winner", step.winner}, {"rounds", fight.Round()}, {"steps", fight.Steps()}, {"fighters", Json::array()}};
        for (const umbra::rpg::Character &c : fight.Characters())
            result["fighters"].push_back({{"id", c.id}, {"health", c.health}, {"energy", c.energy}});
        if (args.json)
            std::cout << result.dump(2) << "\n";
        else
        {
            std::cout << (step.done ? (step.winner == 0 ? "Hero side wins" : "Enemy side wins") : "No result") << " after "
                      << fight.Round() << " round(s), " << fight.Steps() << " action(s)\n";
            for (const umbra::rpg::Character &c : fight.Characters())
                std::cout << "  " << c.id << ": " << c.health << " HP, " << c.energy << " EN\n";
        }
        return step.done ? 0 : 1;
    }

    // A learned enemy model: readable on its own, and still fitting this content (same checks as the game).
    int PolicyCheck(const Arguments &args)
    {
        if (args.positional.size() != 1 || args.content.empty())
            return Usage();
        std::vector<Diagnostic> diagnostics;
        umbra::rpg::Content content;
        umbra::policy::PolicyModel model;
        if (umbra::rpg::LoadContent(args.content, content, diagnostics) && umbra::policy::LoadPolicy(args.positional[0], model, diagnostics))
            umbra::policy::CheckCompatible(model, content, args.positional[0], diagnostics);
        return Report(diagnostics, 1, args.json);
    }
}

int main(int argc, char **argv)
{
    // 1. Command and options.
    if (argc < 2)
        return Usage();
    const std::string command = argv[1];
    Arguments args;
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--assets" && i + 1 < argc)
            args.assets = argv[++i];
        else if (arg == "--content" && i + 1 < argc)
            args.content = argv[++i];
        else if (arg == "--json")
            args.json = true;
        else if (!arg.empty() && arg[0] != '-')
            args.positional.push_back(arg);
        else
            return Usage();
    }

    // 2. Run it.
    if (command == "validate")
        return Validate(args);
    if (command == "inspect")
        return Inspect(args);
    if (command == "schema")
        return Schema(args);
    if (command == "fight")
        return Fight(args);
    if (command == "policy-check")
        return PolicyCheck(args);
    return Usage();
}
