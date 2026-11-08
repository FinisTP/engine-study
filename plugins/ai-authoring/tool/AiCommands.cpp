// UmbraTool draft, texture and model: the AI authoring tools without the editor (local Ollama / ComfyUI only).

#include <ToolCommand.hpp>

#include <Document.hpp>
#include <ModelAssistant.hpp>
#include <QuestAssistant.hpp>
#include <RpgContent.hpp>
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

    // AI texture: idea -> local ComfyUI -> seamless texture + material file(s) in the asset folder.
    int Texture(const Arguments &args)
    {
        if (args.positional.size() != 1 || !args.Has("assets") || args.Int("count", 1) < 1 || args.Int("count", 1) > 8 || !umbra::assets::GoodTextureSize(args.Int("size", 1024), args.Int("size", 1024)) ||
            args.Float("meters", 2.0F) < 0.05F || args.Float("meters", 2.0F) > 100.0F)
            return Usage();
        std::vector<Diagnostic> diagnostics;

        // 1. Where ComfyUI is and which model to use.
        std::vector<std::string> checkpoints;
        const std::string host = !args.Has("host") ? umbra::localai::FindComfyUi(checkpoints) : args.Text("host");
        if (!!args.Has("host"))
            checkpoints = umbra::localai::ListCheckpoints(host, diagnostics);
        const std::string model = !args.Has("model") ? umbra::localai::PreferredCheckpoint(checkpoints) : args.Text("model");
        if (host.empty() || model.empty())
        {
            if (diagnostics.empty())
                diagnostics.push_back({Severity::Error, "ai.connect", "comfyui", "",
                                       host.empty() ? "ComfyUI is not running on port 8000 or 8188. Start ComfyUI." : "ComfyUI has no models in models/checkpoints."});
            return Report(diagnostics, 0, args.json);
        }

        // 2. Generate the variations, printing progress.
        const auto style = umbra::localai::LoadTextureStyle(args.Assets());
        const auto requests = umbra::localai::MakeTextureRequests(args.positional[0], style, model, host, umbra::localai::TextureSeeds(args.Int("count", 1), args.Long("seed", -1)));
        umbra::localai::ImageJob job;
        job.Start(requests);
        std::optional<std::vector<umbra::localai::ImageResult>> results;
        while (!(results = job.Take()))
        {
            if (!args.json)
                std::cerr << "\rGenerating with " << model << "... " << job.Finished() << "/" << requests.size() << ", "
                          << static_cast<int>(job.Seconds()) << " s   " << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        if (!args.json)
            std::cerr << "\n";

        // 3. Each picture: seamless, saved as a new material; a warning if the seam still shows.
        Json made = Json::array();
        for (std::size_t i = 0; i < results->size(); ++i)
        {
            const auto &result = (*results)[i];
            diagnostics.insert(diagnostics.end(), result.diagnostics.begin(), result.diagnostics.end());
            if (!result.ok)
                continue;
            const auto candidate = umbra::localai::FinishTexture(result.image, requests[i], static_cast<std::uint32_t>(args.Int("size", 1024)));
            const std::string path = umbra::localai::SaveTextureMaterial(args.Path("assets"), !args.Has("name") ? args.positional[0] : args.Text("name"), candidate, args.Float("meters", 2.0F), diagnostics);
            if (path.empty())
                continue;
            if (candidate.seam > umbra::assets::visibleSeam)
                diagnostics.push_back({Severity::Warning, "texture.seam", path, "", "the repeat seam may still show (score " + std::to_string(candidate.seam) + ")"});
            const auto license = umbra::assets::ModelLicense(model);
            if (!license.commercial)
                diagnostics.push_back({Severity::Warning, "material.license", path, "generated.model",
                                       license.known ? "made with " + model + " (" + license.name + "): not for a game you sell"
                                                     : "license of " + model + " is unknown: check it before shipping"});
            made.push_back({{"material", path}, {"seed", requests[i].seed}, {"seamBefore", candidate.seamBefore}, {"seam", candidate.seam}});
            if (!args.json)
                std::cout << "Made " << path << " (seed " << requests[i].seed << ", seam " << candidate.seamBefore << " -> " << candidate.seam << ")\n";
        }
        if (args.json)
        {
            std::cout << Json{{"model", model}, {"host", host}, {"made", made}, {"diagnostics", umbra::authoring::ToJson(diagnostics)}}.dump(2) << "\n";
            return made.empty() ? 1 : 0;
        }
        const int code = Report(diagnostics, made.size(), false);
        return made.empty() ? 1 : code;
    }

    // Waits for a background ComfyUI batch, printing progress.
    template <class Job>
    auto Wait(Job &job, const char *what, std::size_t count, bool quiet)
    {
        decltype(job.Take()) results;
        while (!(results = job.Take()))
        {
            if (!quiet)
                std::cerr << "\r" << what << "... " << job.Finished() << "/" << count << ", " << static_cast<int>(job.Seconds()) << " s   " << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        if (!quiet)
            std::cerr << "\n";
        return *results;
    }

    // AI model: idea -> concept picture (SDXL) or a given picture -> exported image-to-3D workflow -> models/<name>.glb.
    int Model(const Arguments &args)
    {
        const int sources = !!args.Has("idea") + !!args.Has("image") + !!args.Has("resume");
        if (!args.Has("assets") || sources != 1 || args.Int("faces", 10000) < 100 || args.Int("faces", 10000) > 1000000)
            return Usage();
        std::vector<Diagnostic> diagnostics;
        const std::filesystem::path workflowFile = !args.Has("workflow") ? umbra::localai::DefaultModelWorkflow(args.Path("assets")) : args.Path("workflow");
        umbra::localai::ModelRequest request;
        if (!umbra::localai::LoadModelWorkflow(workflowFile, request.workflow, diagnostics))
            return Report(diagnostics, 0, args.json);

        // 1. ComfyUI, and the picture: a concept from the idea, or the given file.
        std::vector<std::string> checkpoints;
        request.host = !args.Has("host") ? umbra::localai::FindComfyUi(checkpoints) : args.Text("host");
        if (request.host.empty())
        {
            diagnostics.push_back({Severity::Error, "ai.connect", "comfyui", "", "ComfyUI is not running on port 8000 or 8188. Start ComfyUI."});
            return Report(diagnostics, 0, args.json);
        }
        const long long seed = umbra::localai::TextureSeeds(1, args.Long("seed", -1))[0];
        std::string what = !args.Has("resume") ? "file: " + args.Path("image").filename().string() : "ComfyUI job " + args.Text("resume");
        if (!!args.Has("resume"))
            request.resumeId = args.Text("resume"); // the picture is already in ComfyUI
        else if (!!args.Has("idea"))
        {
            if (!!args.Has("host"))
                checkpoints = umbra::localai::ListCheckpoints(request.host, diagnostics);
            const std::string checkpoint = !args.Has("model") ? umbra::localai::PreferredCheckpoint(checkpoints) : args.Text("model");
            umbra::localai::ImageJob concepts;
            const auto requests = umbra::localai::MakeTextureRequests(args.Text("idea"), umbra::localai::LoadObjectStyle(args.Assets()), checkpoint, request.host, {seed});
            concepts.Start(requests);
            const auto results = Wait(concepts, "Concept picture", 1, args.json);
            diagnostics.insert(diagnostics.end(), results[0].diagnostics.begin(), results[0].diagnostics.end());
            if (!results[0].ok)
                return Report(diagnostics, 0, args.json);
            request.picture = results[0].image;
            what = requests[0].prompt;
        }
        else
        {
            std::string error;
            if (!umbra::assets::LoadImageFile(args.Path("image"), request.picture, error))
            {
                diagnostics.push_back({Severity::Error, "file.read", args.Path("image").string(), "", error});
                return Report(diagnostics, 0, args.json);
            }
        }

        // 2. The 3D model, saved and checked.
        request.seed = seed;
        request.faces = args.Int("faces", 10000);
        if (!args.json)
            request.queued = [&args](const std::string &job) {
                std::cerr << "ComfyUI job " << job << ". It keeps going if this tool is closed; then fetch it with\n  UmbraTool model --resume " << job
                          << " --assets " << args.Text("assets") << (!args.Has("name") ? "" : " --name \"" + args.Text("name") + "\"") << "\n";
            };
        umbra::localai::ModelJob job;
        job.Start({request});
        const auto results = Wait(job, "3D model", 1, args.json);
        diagnostics.insert(diagnostics.end(), results[0].diagnostics.begin(), results[0].diagnostics.end());
        std::string path;
        umbra::localai::ModelInfo info;
        if (results[0].ok)
        {
            const std::string name = !!args.Has("name") ? args.Text("name") : !!args.Has("idea") ? args.Text("idea") : !!args.Has("resume") ? "model" : args.Path("image").stem().string();
            path = umbra::localai::SaveGeneratedModel(args.Path("assets"), name, results[0].glb, request.picture,
                                                      {what, workflowFile.filename().string(), seed, args.Int("faces", 10000)}, results[0].graph, diagnostics);
            if (!path.empty() && umbra::authoring::InspectModel(args.Assets() / path, info, diagnostics) && info.triangles > 2 * static_cast<std::size_t>(args.Int("faces", 10000)))
                diagnostics.push_back({Severity::Warning, "model.size", path, "", std::to_string(info.triangles) + " triangles: the workflow did not reduce the mesh (no DecimateMesh?)"});
        }
        if (args.json)
        {
            std::cout << Json{{"model", path}, {"triangles", info.triangles}, {"textureSize", info.textureSize}, {"seed", seed},
                              {"diagnostics", umbra::authoring::ToJson(diagnostics)}}.dump(2) << "\n";
            return path.empty() ? 1 : 0;
        }
        if (!path.empty())
            std::cout << "Made " << path << ": " << info.triangles << " triangles, texture " << info.textureSize << " px, size "
                      << info.max.x - info.min.x << " x " << info.max.y - info.min.y << " x " << info.max.z - info.min.z << " (model units)\n";
        const int code = Report(diagnostics, path.empty() ? 0 : 1, false);
        return path.empty() ? 1 : code;
    }
}

UMBRA_TOOL_COMMAND(Draft, "draft", "draft <rpg file> --npc <npc> (--idea \"<text>\" | --from <draft file>) [--apply] [--json]\n"
                                    "            [--model <name>] [--host <url>] [--scenes <folder>] [--fix <n>] [--save-draft <file>]", Draft);
UMBRA_TOOL_COMMAND(Texture, "texture", "texture \"<idea>\" --assets <folder> [--name <name>] [--count <n>] [--seed <n>] [--meters <m>]\n"
                                        "            [--size <px>] [--model <checkpoint>] [--host <url>] [--json]", Texture);
UMBRA_TOOL_COMMAND(Model, "model", "model (--idea \"<text>\" | --image <picture> | --resume <job>) --assets <folder> [--name <name>]\n"
                                    "            [--faces <n>] [--seed <n>] [--workflow <api json>] [--model <checkpoint>] [--host <url>] [--json]", Model);
