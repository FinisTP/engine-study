#include "Generate.hpp"

#include <AnimationGen.hpp>
#include <ComfyUi.hpp>
#include <Material.hpp>
#include <ModelCache.hpp>
#include <ModelAssistant.hpp>
#include <ModelTools.hpp>
#include <TextureAssistant.hpp>
#include <TextureTools.hpp>

#include <chrono>
#include <fstream>
#include <iterator>
#include <thread>

namespace umbra::localai
{
    namespace
    {
        using core::Severity;

        // Waits for a background ComfyUI batch, reporting progress and honouring `stop`.
        template <class Job>
        auto Wait(Job &job, const GenerateOptions &options, const std::string &what, std::size_t count)
        {
            decltype(job.Take()) results;
            while (!(results = job.Take()))
            {
                if (options.stop && *options.stop)
                    job.Cancel();
                if (options.progress)
                    options.progress(what, job.Finished(), count, job.Seconds());
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
            return *results;
        }

        // ComfyUI's address and its checkpoints; false (with a diagnostic) if it isn't there.
        bool Connect(const GenerateOptions &options, GenerateResult &out, std::vector<std::string> &checkpoints)
        {
            out.host = options.host.empty() ? FindComfyUi(checkpoints) : options.host;
            if (!options.host.empty())
                checkpoints = ListCheckpoints(options.host, out.diagnostics);
            if (!out.host.empty())
                return true;
            out.diagnostics.push_back({Severity::Error, "ai.connect", "comfyui", "", "ComfyUI is not running on port 8000 or 8188. Start ComfyUI."});
            return false;
        }

        std::string NameOf(const GenerateOptions &options) { return options.name.empty() ? options.idea : options.name; }
    }

    GenerateResult CreateTexture(const GenerateOptions &options)
    {
        GenerateResult out;
        std::vector<std::string> checkpoints;
        // 1. ComfyUI and the model.
        const bool connected = Connect(options, out, checkpoints);
        out.model = options.model.empty() ? PreferredCheckpoint(checkpoints) : options.model;
        if (!connected || out.model.empty())
        {
            if (connected && out.diagnostics.empty())
                out.diagnostics.push_back({Severity::Error, "ai.connect", "comfyui", "", "ComfyUI has no models in models/checkpoints."});
            return out;
        }

        // 2. Generate the variations.
        const auto requests = MakeTextureRequests(options.idea, LoadTextureStyle(options.assets), out.model, out.host, TextureSeeds(options.count, options.seed));
        ImageJob job;
        job.Start(requests);
        const auto results = Wait(job, options, "Generating with " + out.model, requests.size());

        // 3. Each picture: seamless, saved as a new material; a warning if the seam still shows.
        for (std::size_t i = 0; i < results.size(); ++i)
        {
            const auto &result = results[i];
            out.diagnostics.insert(out.diagnostics.end(), result.diagnostics.begin(), result.diagnostics.end());
            if (!result.ok)
                continue;
            const auto candidate = FinishTexture(result.image, requests[i], options.size);
            const std::string path = SaveTextureMaterial(options.assets.Project(), NameOf(options), candidate, options.meters, out.diagnostics);
            if (path.empty())
                continue;
            if (candidate.seam > assets::visibleSeam)
                out.diagnostics.push_back({Severity::Warning, "texture.seam", path, "", "the repeat seam may still show (score " + std::to_string(candidate.seam) + ")"});
            const auto license = assets::ModelLicense(out.model);
            if (!license.commercial)
                out.diagnostics.push_back({Severity::Warning, "material.license", path, "generated.model",
                                           license.known ? "made with " + out.model + " (" + license.name + "): not for a game you sell"
                                                         : "license of " + out.model + " is unknown: check it before shipping"});
            out.made.push_back({path, requests[i].seed, {{"seamBefore", candidate.seamBefore}, {"seam", candidate.seam}}});
        }
        return out;
    }

    GenerateResult CreateSound(const GenerateOptions &options)
    {
        GenerateResult out;
        std::vector<std::string> checkpoints;
        // 1. ComfyUI and the audio model.
        const bool connected = Connect(options, out, checkpoints);
        out.model = options.model.empty() ? PreferredSoundModel(checkpoints, options.kind) : options.model;
        if (!connected)
            return out;
        if (out.model.empty())
        {
            out.diagnostics.push_back({Severity::Error, "ai.model", "comfyui", "",
                                       "ComfyUI has no audio model: ace_step_1.5_turbo_aio.safetensors (music) or stable_audio_3_*.safetensors (effects) in models/checkpoints."});
            return out;
        }

        // 2. Generate, then decode, loop or trim, normalize and save each variation.
        const float seconds = options.seconds > 0.0F ? options.seconds : options.kind == SoundKind::Music ? 60.0F : 2.0F;
        auto requests = MakeSoundRequests(options.idea, options.kind, LoadSoundStyle(options.assets), out.model, out.host, seconds, TextureSeeds(options.count, options.seed));
        for (auto &r : requests)
        {
            r.bpm = options.bpm > 0 ? options.bpm : r.bpm;
            r.keyscale = options.key.empty() ? r.keyscale : options.key;
        }
        AudioJob job;
        job.Start(requests);
        const auto results = Wait(job, options, "Generating with " + out.model, requests.size());
        const std::string name = TextureName(NameOf(options));
        for (const auto &result : results)
        {
            out.diagnostics.insert(out.diagnostics.end(), result.diagnostics.begin(), result.diagnostics.end());
            Pcm pcm;
            std::string error;
            if (!result.ok)
                continue;
            if (!DecodeFlac(result.file, pcm, error))
            {
                out.diagnostics.push_back({Severity::Error, "ai.answer", "comfyui", "", "couldn't read ComfyUI's sound: " + error});
                continue;
            }
            const auto sound = FinishSound(pcm, options.kind, result.request);
            const std::string path = SaveSound(options.assets.Project(), name, options.kind, sound, out.diagnostics);
            if (path.empty())
                continue;
            if (sound.silent)
                out.diagnostics.push_back({Severity::Warning, "sound.silent", path, "", "almost silent: try another prompt or seed"});
            const auto license = assets::ModelLicense(out.model);
            if (!license.known || !license.commercial)
                out.diagnostics.push_back({Severity::Warning, "sound.license", path, "", "license of " + out.model + ": " + (license.known ? license.name : "unknown, check it before shipping")});
            out.made.push_back({path, result.request.seed, {{"seconds", sound.seconds}, {"license", license.name}}});
        }
        return out;
    }

    GenerateResult CreateModel(const GenerateOptions &options)
    {
        GenerateResult out;
        const std::filesystem::path workflowFile = options.workflow.empty() ? DefaultModelWorkflow(options.assets.Project()) : options.workflow;
        ModelRequest request;
        if (!LoadModelWorkflow(workflowFile, request.workflow, out.diagnostics))
            return out;

        // 1. ComfyUI, and the picture: a concept from the idea, a given file, or a job queued earlier.
        std::vector<std::string> checkpoints;
        if (!Connect(options, out, checkpoints))
            return out;
        request.host = out.host;
        const long long seed = TextureSeeds(1, options.seed)[0];
        std::string what = !options.resume.empty() ? "ComfyUI job " + options.resume : "file: " + options.image.filename().string();
        if (!options.resume.empty())
            request.resumeId = options.resume; // the picture is already in ComfyUI
        else if (options.image.empty())
        {
            out.model = options.model.empty() ? PreferredCheckpoint(checkpoints) : options.model;
            ImageJob concepts;
            const auto requests = MakeTextureRequests(options.idea, LoadObjectStyle(options.assets), out.model, request.host, {seed});
            concepts.Start(requests);
            const auto results = Wait(concepts, options, "Concept picture", 1);
            out.diagnostics.insert(out.diagnostics.end(), results[0].diagnostics.begin(), results[0].diagnostics.end());
            if (!results[0].ok)
                return out;
            request.picture = results[0].image;
            what = requests[0].prompt;
        }
        else
        {
            std::string error;
            if (!assets::LoadImageFile(options.image, request.picture, error))
            {
                out.diagnostics.push_back({Severity::Error, "file.read", options.image.string(), "", error});
                return out;
            }
        }

        // 2. The 3D model, saved and checked.
        request.seed = seed;
        request.faces = options.faces;
        request.queued = options.queued;
        ModelJob job;
        job.Start({request});
        const auto results = Wait(job, options, "3D model", 1);
        out.diagnostics.insert(out.diagnostics.end(), results[0].diagnostics.begin(), results[0].diagnostics.end());
        if (!results[0].ok)
            return out;
        const std::string name = !options.name.empty() ? options.name : !options.idea.empty() ? options.idea : !options.resume.empty() ? "model" : options.image.stem().string();
        const std::string path = SaveGeneratedModel(options.assets.Project(), name, results[0].glb, request.picture, {what, workflowFile.filename().string(), seed, options.faces},
                                                    results[0].graph, out.diagnostics);
        authoring::ModelInfo info;
        if (path.empty() || !authoring::InspectModel(options.assets / path, info, out.diagnostics))
            return out;
        if (info.triangles > 2 * static_cast<std::size_t>(options.faces))
            out.diagnostics.push_back({Severity::Warning, "model.size", path, "", std::to_string(info.triangles) + " triangles: the workflow did not reduce the mesh (no DecimateMesh?)"});
        out.made.push_back({path, seed,
                            {{"triangles", info.triangles}, {"textureSize", info.textureSize}, {"size", {info.max.x - info.min.x, info.max.y - info.min.y, info.max.z - info.min.z}}}});
        return out;
    }

    namespace
    {
        // One rig or motion run: workflow, ComfyUI, the source model, the run, FBX -> GLB, saved and checked.
        GenerateResult CreateAnim(const GenerateOptions &options, AnimKind kind)
        {
            GenerateResult out;
            const std::filesystem::path workflowFile = options.workflow.empty() ? DefaultAnimWorkflow(options.assets.Project(), kind) : options.workflow;
            AnimRequest request;
            request.kind = kind;
            if (!LoadAnimWorkflow(workflowFile, kind, request.workflow, out.diagnostics))
                return out;
            std::vector<std::string> checkpoints;
            if (!Connect(options, out, checkpoints))
                return out;
            request.host = out.host;
            request.seed = TextureSeeds(1, options.seed)[0];
            request.prompt = options.idea;
            request.seconds = options.seconds > 0.0F ? options.seconds : 4.0F;
            if (!options.source.empty())
            {
                std::ifstream in(options.assets / options.source, std::ios::binary);
                if (!assets::IsSafeAssetPath(options.source) || !in)
                {
                    out.diagnostics.push_back({Severity::Error, "file.read", options.source, "", "cannot read the model"});
                    return out;
                }
                request.model.assign(std::istreambuf_iterator<char>(in), {});
                request.modelName = std::filesystem::path(options.source).filename().string();
            }

            // The run (in the background so `stop` and progress work), then FBX -> GLB if needed.
            BatchJob<AnimRequest, AnimResult, GenerateAnim> job;
            job.Start({request});
            const auto results = Wait(job, options, kind == AnimKind::Rig ? "Rigging" : "Motion", 1);
            out.diagnostics.insert(out.diagnostics.end(), results[0].diagnostics.begin(), results[0].diagnostics.end());
            if (!results[0].ok)
                return out;
            std::vector<std::uint8_t> glb = results[0].file;
            if (results[0].extension == ".fbx" && !FbxToGlb(results[0].file, glb, out.diagnostics))
                return out;

            // Saved next to the other assets with how it was made.
            const std::string stem = std::filesystem::path(options.source).stem().string();
            const std::string name = !options.name.empty() ? options.name : kind == AnimKind::Rig ? stem + "_rigged" : options.idea;
            const nlohmann::ordered_json source{{"workflow", workflowFile.filename().string()}, {"source", options.source}, {"prompt", options.idea},
                                                {"seconds", kind == AnimKind::Motion ? request.seconds : 0.0F}, {"seed", request.seed}};
            const std::string path = SaveAnimResult(options.assets.Project(), kind, name, glb, source, out.diagnostics);
            authoring::ModelInfo info;
            if (path.empty() || !authoring::InspectModel(options.assets / path, info, out.diagnostics))
                return out;
            out.made.push_back({path, request.seed, {{"joints", info.joints}, {"clips", info.clips}}});
            return out;
        }
    }

    GenerateResult CreateRig(const GenerateOptions &options)
    {
        if (options.source.empty())
            return {{}, {}, {}, {{Severity::Error, "ai.answer", "rig", "", "no model to rig (--model-file models/<name>.glb)"}}};
        return CreateAnim(options, AnimKind::Rig);
    }

    GenerateResult CreateMotion(const GenerateOptions &options)
    {
        if (options.idea.empty())
            return {{}, {}, {}, {{Severity::Error, "ai.answer", "motion", "", "describe the motion (--idea \"<text>\")"}}};
        return CreateAnim(options, AnimKind::Motion);
    }
}
