// UmbraTool: content checks and test runs without a window, GPU or editor.
// Every command prints the same diagnostics the editor and the game use. See README.md.
//
//   UmbraTool validate <file>... [--assets <folder>] [--content <rpg file>] [--json]
//   UmbraTool inspect  <file> [--json]
//   UmbraTool schema   [--json]
//   UmbraTool fight    <rpg file> <hero actor> <enemy actor>... [--json]
//   UmbraTool policy-check <policy file> --content <rpg file> [--json]
//   UmbraTool draft <rpg file> --npc <npc> (--idea "<text>" | --from <draft file>) [--apply] [--json]
//             [--model <name>] [--host <url>] [--scenes <folder>] [--fix <n>] [--save-draft <file>]
//
// Exit code: 0 = ok, 1 = errors found (or the fight couldn't run), 2 = bad usage.

#include <Document.hpp>
#include <PolicyModel.hpp>
#include <QuestAssistant.hpp>
#include <RpgContent.hpp>
#include <SaveGame.hpp>
#include <SceneFile.hpp>
#include <TurnEnvironment.hpp>
#include <Validate.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <thread>
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
        // draft
        std::string npc, idea, model, host = "http://localhost:11434"; // empty model: an installed one
        std::filesystem::path from, scenes, saveDraft;
        int fix = 1;
        bool apply = false;
    };

    int Usage()
    {
        std::cerr << "usage:\n"
                     "  UmbraTool validate <file>... [--assets <folder>] [--content <rpg file>] [--json]\n"
                     "  UmbraTool inspect  <file> [--json]\n"
                     "  UmbraTool schema   [--json]\n"
                     "  UmbraTool fight    <rpg file> <hero actor> <enemy actor>... [--json]\n"
                     "  UmbraTool policy-check <policy file> --content <rpg file> [--json]\n"
                     "  UmbraTool draft <rpg file> --npc <npc> (--idea \"<text>\" | --from <draft file>) [--apply] [--json]\n"
                     "            [--model <name>] [--host <url>] [--scenes <folder>] [--fix <n>] [--save-draft <file>]\n";
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

    // Asks the local model, printing progress; false with diagnostics on failure.
    bool Ask(const umbra::localai::ChatRequest &request, std::string &answer, std::vector<Diagnostic> &diagnostics, bool quiet)
    {
        umbra::localai::ChatJob job;
        job.Start(request);
        std::optional<umbra::localai::ChatResult> result;
        while (!(result = job.Take()))
        {
            if (!quiet)
                std::cerr << "\rAsking " << request.model << "... " << static_cast<int>(job.Seconds()) << " s, " << job.Received() << " characters   " << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        if (!quiet)
            std::cerr << "\n";
        diagnostics.insert(diagnostics.end(), result->diagnostics.begin(), result->diagnostics.end());
        answer = result->text;
        return result->ok;
    }

    // AI quest draft: idea -> local model -> validated, played-through additions -> (--apply) saved.
    int Draft(const Arguments &args)
    {
        if (args.positional.size() != 1 || args.npc.empty() || args.idea.empty() == args.from.empty())
            return Usage();
        const std::filesystem::path file = args.positional[0];
        umbra::authoring::Document doc(file, umbra::authoring::RpgValidator());
        std::vector<Diagnostic> diagnostics;
        if (!doc.Open(diagnostics))
            return Report(diagnostics, 1, args.json);
        const auto world = umbra::authoring::ScanScenes(args.scenes.empty() ? file.parent_path().parent_path() / "scenes" : args.scenes);

        // 1. A draft: from a file, or from the model (asking it to fix problems up to --fix times).
        umbra::authoring::DraftPlan plan;
        std::string answer;
        std::string model = args.model;
        if (model.empty() && args.from.empty())
        {
            std::vector<Diagnostic> ignored; // a missing Ollama is reported by the request itself
            model = umbra::localai::PreferredModel(umbra::localai::ListModels(args.host, ignored), "qwen2.5:7b");
        }
        umbra::localai::ChatRequest request = umbra::localai::MakeDraftRequest(doc.Data(), args.npc, args.idea, model, args.host);
        for (int attempt = 0; attempt <= args.fix; ++attempt)
        {
            diagnostics.clear();
            if (!args.from.empty())
            {
                std::ifstream in(args.from);
                answer.assign(std::istreambuf_iterator<char>(in), {});
            }
            else if (!Ask(request, answer, diagnostics, args.json))
                break; // provider problems aren't fixed by asking again
            umbra::authoring::Json draft;
            if (umbra::localai::ParseDraft(answer, draft, diagnostics) && umbra::authoring::PlanDraft(doc.Data(), draft, args.npc, world, plan, diagnostics))
                break;
            if (!args.from.empty() || attempt == args.fix)
                break;
            if (!args.json)
                std::cerr << "The draft has problems; asking the model to fix them.\n";
            umbra::localai::AddFixRequest(request, answer, diagnostics);
        }
        if (!args.saveDraft.empty() && !answer.empty())
            std::ofstream(args.saveDraft) << answer;

        // 2. Apply only when asked and clean; keep the old file next to it for undo.
        const bool clean = !plan.changes.empty() && !umbra::core::HasErrors(diagnostics);
        bool applied = false;
        if (args.apply && clean)
        {
            std::ifstream in(file, std::ios::binary);
            const std::string before(std::istreambuf_iterator<char>(in), {});
            applied = doc.Apply(plan.changes, plan.label, diagnostics, doc.Revision()) && doc.Save(diagnostics);
            if (applied)
                std::ofstream(file.string() + ".before-draft", std::ios::binary) << before; // the exact old bytes
        }

        // 3. Report: what would be added, the play-through, the problems.
        if (args.json)
        {
            Json changes = Json::array();
            for (const auto &c : plan.changes)
                changes.push_back({{"op", c.op == umbra::authoring::Change::Op::Insert ? "insert" : "set"}, {"path", c.path}, {"value", Json::parse(c.value.dump())}});
            std::cout << Json{{"quest", plan.quest}, {"conversation", plan.conversation}, {"preview", plan.preview}, {"play", plan.play},
                              {"changes", changes}, {"applied", applied}, {"diagnostics", umbra::authoring::ToJson(diagnostics)}}.dump(2)
                      << "\n";
            return clean ? 0 : 1;
        }
        for (const std::string &line : plan.preview)
            std::cout << line << "\n";
        if (!plan.play.empty())
            std::cout << "Play-through:\n";
        for (const std::string &line : plan.play)
            std::cout << "  " << line << "\n";
        const int code = Report(diagnostics, 1, false);
        if (applied)
            std::cout << "Applied to " << file.string() << ". Undo: copy " << file.string() << ".before-draft back, or use the editor.\n";
        else if (clean)
            std::cout << "Nothing changed. Add --apply to add this quest.\n";
        return code;
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
        else if (arg == "--apply")
            args.apply = true;
        else if (i + 1 < argc && (arg == "--npc" || arg == "--idea" || arg == "--model" || arg == "--host" || arg == "--from" ||
                                  arg == "--scenes" || arg == "--save-draft" || arg == "--fix"))
        {
            const std::string value = argv[++i];
            if (arg == "--npc")
                args.npc = value;
            else if (arg == "--idea")
                args.idea = value;
            else if (arg == "--model")
                args.model = value;
            else if (arg == "--host")
                args.host = value;
            else if (arg == "--from")
                args.from = value;
            else if (arg == "--scenes")
                args.scenes = value;
            else if (arg == "--save-draft")
                args.saveDraft = value;
            else if (value.find_first_not_of("0123456789") != std::string::npos || value.empty() || value.size() > 1)
                return Usage();
            else
                args.fix = std::stoi(value);
        }
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
    if (command == "draft")
        return Draft(args);
    return Usage();
}
