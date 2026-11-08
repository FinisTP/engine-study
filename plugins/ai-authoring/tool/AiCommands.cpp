// UmbraTool draft, texture, model and sound: the AI authoring tools without the editor (local Ollama / ComfyUI only).

#include <ToolCommand.hpp>

#include <Document.hpp>
#include <Generate.hpp>
#include <ModelAssistant.hpp>
#include <QuestAssistant.hpp>
#include <RpgContent.hpp>
#include <SoundAssistant.hpp>
#include <TextureAssistant.hpp>
#include <TextureTools.hpp>
#include <Validate.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <optional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using umbra::core::Diagnostic;
    using umbra::core::Severity;
    using umbra::tool::Arguments;
    using umbra::tool::Report;
    using umbra::tool::Usage;
    using Json = nlohmann::json;

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
        if (args.positional.size() != 1 || args.Text("npc").empty() || !args.Has("idea") == !args.Has("from"))
            return Usage();
        const std::filesystem::path file = args.positional[0];
        umbra::authoring::Document doc(file, umbra::authoring::RpgValidator());
        std::vector<Diagnostic> diagnostics;
        if (!doc.Open(diagnostics))
            return Report(diagnostics, 1, args.json);
        const auto world = umbra::authoring::ScanScenes(!args.Has("scenes") ? file.parent_path().parent_path() / "scenes" : args.Path("scenes"));

        // 1. A draft: from a file, or from the model (asking it to fix problems up to --fix times).
        umbra::authoring::DraftPlan plan;
        std::string answer;
        const std::string host = !args.Has("host") ? "http://localhost:11434" : args.Text("host");
        std::string model = args.Text("model");
        if (model.empty() && !args.Has("from"))
        {
            std::vector<Diagnostic> ignored; // a missing Ollama is reported by the request itself
            model = umbra::localai::PreferredModel(umbra::localai::ListModels(host, ignored), "qwen2.5:7b");
        }
        umbra::localai::ChatRequest request = umbra::localai::MakeDraftRequest(doc.Data(), args.Text("npc"), args.Text("idea"), model, host);
        for (int attempt = 0; attempt <= args.Int("fix", 1); ++attempt)
        {
            diagnostics.clear();
            if (!!args.Has("from"))
            {
                std::ifstream in(args.Path("from"));
                answer.assign(std::istreambuf_iterator<char>(in), {});
            }
            else if (!Ask(request, answer, diagnostics, args.json))
                break; // provider problems aren't fixed by asking again
            umbra::authoring::Json draft;
            if (umbra::localai::ParseDraft(answer, draft, diagnostics) && umbra::authoring::PlanDraft(doc.Data(), draft, args.Text("npc"), world, plan, diagnostics))
                break;
            if (!!args.Has("from") || attempt == args.Int("fix", 1))
                break;
            if (!args.json)
                std::cerr << "The draft has problems; asking the model to fix them.\n";
            umbra::localai::AddFixRequest(request, answer, diagnostics);
        }
        if (!!args.Has("save-draft") && !answer.empty())
            std::ofstream(args.Path("save-draft")) << answer;

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

    // Options shared by texture, sound and model; progress on stderr unless --json.
    umbra::localai::GenerateOptions Options(const Arguments &args, const std::string &idea)
    {
        umbra::localai::GenerateOptions options;
        options.idea = idea;
        options.name = args.Text("name");
        options.assets = args.Assets();
        options.host = args.Text("host");
        options.model = args.Text("model");
        options.count = args.Int("count", 1);
        options.seed = args.Long("seed", -1);
        if (!args.json)
            options.progress = [](const std::string &what, std::size_t done, std::size_t total, float seconds) {
                std::cerr << "\r" << what << "... " << done << "/" << total << ", " << static_cast<int>(seconds) << " s   " << std::flush;
            };
        return options;
    }

    // Prints what was made (text or JSON); the exit code.
    int Finish(const Arguments &args, const umbra::localai::GenerateResult &result, const char *key)
    {
        if (!args.json)
            std::cerr << "\n";
        if (args.json)
        {
            Json made = Json::array();
            for (const auto &m : result.made)
            {
                Json entry = {{key, m.path}, {"seed", m.seed}};
                entry.update(m.info);
                made.push_back(entry);
            }
            Json out = {{"model", result.model}, {"host", result.host}, {"made", made}, {"diagnostics", umbra::authoring::ToJson(result.diagnostics)}};
            std::cout << out.dump(2) << "\n";
            return result.made.empty() ? 1 : 0;
        }
        for (const auto &m : result.made)
            std::cout << "Made " << m.path << " (seed " << m.seed << ", " << m.info.dump() << ")\n";
        const int code = Report(result.diagnostics, result.made.size(), false);
        return result.made.empty() ? 1 : code;
    }

    // AI texture: idea -> local ComfyUI -> seamless texture + material file(s) in the asset folder.
    int Texture(const Arguments &args)
    {
        if (args.positional.size() != 1 || !args.Has("assets") || args.Int("count", 1) < 1 || args.Int("count", 1) > 8 || !umbra::assets::GoodTextureSize(args.Int("size", 1024), args.Int("size", 1024)) ||
            args.Float("meters", 2.0F) < 0.05F || args.Float("meters", 2.0F) > 100.0F)
            return Usage();
        auto options = Options(args, args.positional[0]);
        options.meters = args.Float("meters", 2.0F);
        options.size = static_cast<std::uint32_t>(args.Int("size", 1024));
        return Finish(args, umbra::localai::CreateTexture(options), "material");
    }

    // Music (a looping .ogg) or a sound effect (a .wav) through a local ComfyUI (S4, SoundGen.md).
    int Sound(const Arguments &args)
    {
        const std::string kindText = args.Text("kind", "music");
        const auto kind = kindText == "effect" ? umbra::localai::SoundKind::Effect : umbra::localai::SoundKind::Music;
        const float seconds = args.Float("seconds", kind == umbra::localai::SoundKind::Music ? 60.0F : 2.0F);
        if (args.positional.size() != 1 || !args.Has("assets") || (kindText != "music" && kindText != "effect") || args.Int("count", 1) < 1 ||
            args.Int("count", 1) > 8 || seconds < 0.5F || seconds > 600.0F)
            return Usage();
        auto options = Options(args, args.positional[0]);
        options.kind = kind;
        options.seconds = seconds;
        options.bpm = args.Int("bpm", 0);
        options.key = args.Text("key");
        return Finish(args, umbra::localai::CreateSound(options), "sound");
    }

    // AI model: idea -> concept picture (SDXL) or a given picture -> exported image-to-3D workflow -> models/<name>.glb.
    int Model(const Arguments &args)
    {
        const int sources = !!args.Has("idea") + !!args.Has("image") + !!args.Has("resume");
        if (!args.Has("assets") || sources != 1 || args.Int("faces", 10000) < 100 || args.Int("faces", 10000) > 1000000)
            return Usage();
        auto options = Options(args, args.Text("idea"));
        options.image = args.Path("image");
        options.resume = args.Text("resume");
        options.workflow = args.Path("workflow");
        options.faces = args.Int("faces", 10000);
        if (!args.json)
            options.queued = [&args](const std::string &job) {
                std::cerr << "\nComfyUI job " << job << ". It keeps going if this tool is closed; then fetch it with\n  UmbraTool model --resume " << job
                          << " --assets " << args.Text("assets") << (!args.Has("name") ? "" : " --name \"" + args.Text("name") + "\"") << "\n";
            };
        return Finish(args, umbra::localai::CreateModel(options), "model");
    }
}

UMBRA_TOOL_COMMAND(Draft, "draft", "draft <rpg file> --npc <npc> (--idea \"<text>\" | --from <draft file>) [--apply] [--json]\n"
                                    "            [--model <name>] [--host <url>] [--scenes <folder>] [--fix <n>] [--save-draft <file>]", Draft);
UMBRA_TOOL_COMMAND(Texture, "texture", "texture \"<idea>\" --assets <folder> [--name <name>] [--count <n>] [--seed <n>] [--meters <m>]\n"
                                        "            [--size <px>] [--model <checkpoint>] [--host <url>] [--json]", Texture);
UMBRA_TOOL_COMMAND(Model, "model", "model (--idea \"<text>\" | --image <picture> | --resume <job>) --assets <folder> [--name <name>]\n"
                                    "            [--faces <n>] [--seed <n>] [--workflow <api json>] [--model <checkpoint>] [--host <url>] [--json]", Model);
UMBRA_TOOL_COMMAND(Sound, "sound", "sound \"<idea>\" --assets <folder> [--kind music|effect] [--seconds <s>] [--name <name>] [--count <n>]\n"
                                    "            [--seed <n>] [--bpm <n>] [--key \"D minor\"] [--model <checkpoint>] [--host <url>] [--json]", Sound);
