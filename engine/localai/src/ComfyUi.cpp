#include <ComfyUi.hpp>

#include <TextureTools.hpp>

#include <httplib.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>

namespace umbra::localai
{
    namespace
    {
        using Json = nlohmann::json;
        using Diagnostics = std::vector<core::Diagnostic>;

        void Err(Diagnostics &d, const std::string &code, const std::string &message)
        {
            d.push_back({core::Severity::Error, code, "comfyui", "", message});
        }

        // ComfyUI lists choices as [[...], {...}] or, newer, ["COMBO", {"options": [...]}].
        std::vector<std::string> Choices(const Json &input)
        {
            std::vector<std::string> names;
            const Json *list = nullptr;
            if (input.is_array() && !input.empty() && input[0].is_array())
                list = &input[0];
            else if (input.is_array() && input.size() > 1 && input[1].is_object() && input[1].contains("options"))
                list = &input[1]["options"];
            if (list != nullptr)
                for (const Json &name : *list)
                    if (name.is_string())
                        names.push_back(name.get<std::string>());
            return names;
        }

        // The first error text ComfyUI gives when it refuses a graph.
        std::string Refusal(const Json &reply)
        {
            std::string text;
            if (reply.contains("node_errors") && reply["node_errors"].is_object())
                for (const auto &[node, info] : reply["node_errors"].items())
                    for (const Json &e : info.value("errors", Json::array()))
                        if (text.empty())
                            text = e.value("message", "") + ": " + e.value("details", "");
            if (text.empty() && reply.contains("error") && reply["error"].is_object())
                text = reply["error"].value("message", "") + " " + reply["error"].value("details", "");
            return text;
        }

        // Which saved file of a finished run to download: {filename, subfolder, type} or null.
        using Pick = std::function<Json(const Json &outputs)>;

        struct Run final
        {
            bool ok = false;
            std::string bytes;
            Diagnostics diagnostics;
        };

        // Queue -> wait (twice a second) -> download one output file. Shared by pictures and models.
        Run RunGraph(const std::string &url, const Json &graph, CancelToken &cancel, int timeoutSeconds, std::size_t maxBytes,
                     const Pick &pick, const std::function<std::string(const std::string &)> &refusalMessage)
        {
            Run run;
            std::string host;
            int port = 0;
            if (!ParseLocalUrl(url, host, port))
            {
                Err(run.diagnostics, "ai.host", "'" + url + "' is not this computer. Umbra only uses a local ComfyUI; there is no cloud fallback.");
                return run;
            }
            httplib::Client client(host, port);
            client.set_connection_timeout(3);
            client.set_read_timeout(60);
            if (!cancel.SetStop([&client] { client.stop(); }))
            {
                Err(run.diagnostics, "ai.cancel", "Cancelled.");
                return run;
            }
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
            auto finish = [&] {
                cancel.SetStop(nullptr);
                if (cancel.Cancelled())
                {
                    httplib::Client(host, port).Post("/interrupt"); // stop ComfyUI working on it
                    run.diagnostics = {{core::Severity::Error, "ai.cancel", "comfyui", "", "Cancelled."}};
                    run.ok = false;
                }
                return run;
            };

            // 1. Queue.
            const httplib::Result queued = client.Post("/prompt", Json{{"prompt", graph}, {"client_id", "umbra"}}.dump(), "application/json");
            if (!queued)
            {
                Err(run.diagnostics, "ai.connect", "ComfyUI is not running at " + url + " (" + httplib::to_string(queued.error()) +
                                                       "). Start ComfyUI (Desktop: port 8000; command line: 8188).");
                return finish();
            }
            const Json reply = Json::parse(queued->body, nullptr, false);
            if (queued->status != 200 || !reply.is_object() || !reply.contains("prompt_id"))
            {
                const std::string why = reply.is_object() ? Refusal(reply) : "";
                const std::string special = refusalMessage(why);
                if (!special.empty())
                    Err(run.diagnostics, "ai.model", special);
                else
                    Err(run.diagnostics, "ai.comfyui", "ComfyUI refused the request (HTTP " + std::to_string(queued->status) + "): " + why);
                return finish();
            }
            const std::string id = reply["prompt_id"].get<std::string>();

            // 2. Wait until the history has it.
            Json entry;
            while (!cancel.Cancelled())
            {
                if (std::chrono::steady_clock::now() > deadline)
                {
                    Err(run.diagnostics, "ai.timeout", "No result within " + std::to_string(timeoutSeconds) + " seconds.");
                    httplib::Client(host, port).Post("/interrupt");
                    return finish();
                }
                const httplib::Result history = client.Get("/history/" + id);
                const Json all = history && history->status == 200 ? Json::parse(history->body, nullptr, false) : Json();
                if (all.is_object() && all.contains(id))
                {
                    entry = all[id];
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            if (cancel.Cancelled())
                return finish();
            const Json status = entry.value("status", Json::object());
            if (status.value("status_str", "") == "error")
            {
                std::string why = "the run failed";
                for (const Json &message : status.value("messages", Json::array()))
                    if (message.is_array() && message.size() == 2 && message[0] == "execution_error")
                        why = message[1].value("exception_message", why);
                Err(run.diagnostics, "ai.comfyui", "ComfyUI: " + why);
                return finish();
            }

            // 3. Download the file the caller wants.
            const Json file = pick(entry.value("outputs", Json::object()));
            if (!file.is_object())
            {
                Err(run.diagnostics, "ai.answer", "ComfyUI finished without the expected file.");
                return finish();
            }
            const httplib::Params params{{"filename", file.value("filename", "")}, {"subfolder", file.value("subfolder", "")},
                                         {"type", file.value("type", "output")}};
            const httplib::Result got = client.Get("/view", params, httplib::Headers{}, [&](const char *data, std::size_t size) {
                run.bytes.append(data, size);
                return run.bytes.size() <= maxBytes && !cancel.Cancelled();
            });
            if (cancel.Cancelled())
                return finish();
            if (run.bytes.size() > maxBytes)
                Err(run.diagnostics, "ai.limit", "The result was larger than " + std::to_string(maxBytes / (1024 * 1024)) + " MiB and was refused.");
            else if (!got || got->status != 200)
                Err(run.diagnostics, "ai.answer", "Couldn't download ComfyUI's result.");
            else
                run.ok = true;
            return finish();
        }

        bool EndsWithGlb(std::string name)
        {
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return name.ends_with(".glb");
        }

        // Sets a node input: a literal value, or the value of a Primitive node wired into it.
        void SetInput(Json &graph, const std::string &classType, const std::string &input, const Json &value)
        {
            for (auto &[id, node] : graph.items())
            {
                if (node.value("class_type", "") != classType || !node.contains("inputs") || !node["inputs"].contains(input))
                    continue;
                Json &slot = node["inputs"][input];
                if (!slot.is_array())
                    slot = value;
                else if (slot.size() == 2 && slot[0].is_string() && graph.contains(slot[0].get<std::string>()))
                {
                    Json &source = graph[slot[0].get<std::string>()];
                    if (source.value("class_type", "").starts_with("Primitive") && source["inputs"].contains("value"))
                        source["inputs"]["value"] = value;
                }
            }
        }
    }

    Json TextureWorkflow(const ImageRequest &r)
    {
        // Node ids are strings; an input [id, n] means "output n of node id".
        auto node = [](const char *type, Json inputs) { return Json{{"class_type", type}, {"inputs", std::move(inputs)}}; };
        return Json{
            {"1", node("CheckpointLoaderSimple", {{"ckpt_name", r.model}})},
            {"2", node("CLIPTextEncode", {{"text", r.prompt}, {"clip", {"1", 1}}})},
            {"3", node("CLIPTextEncode", {{"text", r.negative}, {"clip", {"1", 1}}})},
            {"4", node("EmptyLatentImage", {{"width", r.width}, {"height", r.height}, {"batch_size", 1}})},
            {"5", node("KSampler", {{"seed", r.seed}, {"steps", r.steps}, {"cfg", r.cfg}, {"sampler_name", "dpmpp_2m"}, {"scheduler", "karras"},
                                    {"denoise", 1.0}, {"model", {"1", 0}}, {"positive", {"2", 0}}, {"negative", {"3", 0}}, {"latent_image", {"4", 0}}})},
            {"6", node("VAEDecode", {{"samples", {"5", 0}}, {"vae", {"1", 2}}})},
            {"7", node("PreviewImage", {{"images", {"6", 0}}})}, // a temporary file: nothing piles up in ComfyUI's output
        };
    }

    ImageResult GenerateImage(const ImageRequest &request, CancelToken &cancel)
    {
        const Pick preview = [](const Json &outputs) {
            const Json images = outputs.value("7", Json::object()).value("images", Json::array());
            return images.empty() ? Json() : images[0];
        };
        const auto checkpoint = [&](const std::string &why) {
            return why.find("ckpt_name") != std::string::npos
                       ? "ComfyUI has no model '" + request.model + "'. Put it in ComfyUI's models/checkpoints folder, or pick an installed one."
                       : std::string();
        };
        Run run = RunGraph(request.host, TextureWorkflow(request), cancel, request.timeoutSeconds, request.maxBytes, preview, checkpoint);
        ImageResult result;
        result.diagnostics = run.diagnostics;
        std::string error;
        if (run.ok && !assets::DecodeImage({run.bytes.begin(), run.bytes.end()}, result.image, error))
            Err(result.diagnostics, "ai.answer", "Couldn't read ComfyUI's picture: " + error);
        result.ok = run.ok && error.empty();
        return result;
    }

    bool LoadModelWorkflow(const std::filesystem::path &file, Json &graph, std::vector<core::Diagnostic> &diagnostics)
    {
        auto fail = [&](const std::string &code, const std::string &message) {
            diagnostics.push_back({core::Severity::Error, code, file.string(), "", message});
            return false;
        };
        std::ifstream in(file);
        if (!in)
            return fail("workflow.missing", "no workflow file: export the image-to-3D workflow from ComfyUI with Workflow -> Export (API) and save it here");
        graph = Json::parse(in, nullptr, false);
        if (graph.is_object() && graph.contains("nodes") && graph["nodes"].is_array())
            return fail("workflow.format", "this is a ComfyUI UI workflow; export it with Workflow -> Export (API) instead");
        if (!graph.is_object() || graph.empty())
            return fail("workflow.format", "not a ComfyUI API workflow (JSON object of nodes)");
        int images = 0, saves = 0;
        for (const auto &[id, node] : graph.items())
        {
            if (!node.is_object() || !node.contains("class_type") || !node.contains("inputs"))
                return fail("workflow.format", "node " + id + " has no class_type/inputs: export with Workflow -> Export (API)");
            const std::string type = node["class_type"].get<std::string>();
            images += type == "LoadImage";
            saves += type.starts_with("Save3D") || type == "SaveGLB";
        }
        if (images != 1)
            return fail("workflow.image", "the workflow needs exactly one LoadImage node (it has " + std::to_string(images) + ")");
        if (saves == 0)
            return fail("workflow.output", "the workflow saves no 3D file (add Save 3D or Save GLB)");
        return true;
    }

    Json PrepareModelWorkflow(const ModelRequest &request, const std::string &uploadedPicture)
    {
        Json graph = request.workflow;
        long long seed = request.seed;
        for (auto &[id, node] : graph.items())
        {
            Json &inputs = node["inputs"];
            if (node.value("class_type", "") == "LoadImage")
                inputs["image"] = uploadedPicture;
            for (const char *key : {"seed", "noise_seed"})
                if (inputs.contains(key) && inputs[key].is_number_integer())
                    inputs[key] = seed++; // each sampler its own seed, all from this run's seed
        }
        SetInput(graph, "DecimateMesh", "target_face_count", request.faces);
        SetInput(graph, "BakeTextureFromVoxel", "texture_size", request.textureSize);
        SetInput(graph, "UnwrapMesh", "resolution", request.textureSize);
        return graph;
    }

    ModelResult GenerateModel(const ModelRequest &request, CancelToken &cancel)
    {
        ModelResult result;
        std::string host;
        int port = 0;
        if (!ParseLocalUrl(request.host, host, port))
        {
            Err(result.diagnostics, "ai.host", "'" + request.host + "' is not this computer. Umbra only uses a local ComfyUI; there is no cloud fallback.");
            return result;
        }

        // 1. Upload the picture into ComfyUI's input folder.
        std::vector<std::uint8_t> png;
        if (!assets::EncodePng(request.picture, png))
        {
            Err(result.diagnostics, "ai.answer", "Couldn't encode the picture.");
            return result;
        }
        httplib::Client client(host, port);
        client.set_connection_timeout(3);
        client.set_read_timeout(60);
        const httplib::UploadFormDataItems form{{"image", std::string(png.begin(), png.end()), "umbra_model_input.png", "image/png"},
                                                {"overwrite", "true", "", ""},
                                                {"type", "input", "", ""}};
        const httplib::Result uploaded = client.Post("/upload/image", form);
        const Json reply = uploaded && uploaded->status == 200 ? Json::parse(uploaded->body, nullptr, false) : Json();
        if (!reply.is_object() || !reply.contains("name"))
        {
            Err(result.diagnostics, uploaded ? "ai.comfyui" : "ai.connect",
                uploaded ? "ComfyUI refused the picture upload." : "ComfyUI is not running at " + request.host + ". Start ComfyUI (Desktop: port 8000; command line: 8188).");
            return result;
        }
        const std::string subfolder = reply.value("subfolder", "");
        const std::string name = (subfolder.empty() ? "" : subfolder + "/") + reply["name"].get<std::string>();

        // 2. Run the workflow and download the GLB it saved.
        const Pick glb = [](const Json &outputs) {
            for (const auto &[node, output] : outputs.items())
                for (const auto &[key, list] : output.items())
                    if (list.is_array())
                        for (const Json &file : list)
                            if (file.is_object() && EndsWithGlb(file.value("filename", "")))
                                return file;
            return Json();
        };
        const auto missing = [](const std::string &why) {
            return why.find("not in") != std::string::npos ? "A model the workflow needs isn't installed in ComfyUI: " + why : std::string();
        };
        Run run = RunGraph(request.host, PrepareModelWorkflow(request, name), cancel, request.timeoutSeconds, request.maxBytes, glb, missing);
        result.diagnostics = run.diagnostics;
        result.ok = run.ok;
        result.glb.assign(run.bytes.begin(), run.bytes.end());
        return result;
    }

    std::vector<std::string> ListCheckpoints(const std::string &url, std::vector<core::Diagnostic> &diagnostics)
    {
        std::string host;
        int port = 0;
        if (!ParseLocalUrl(url, host, port))
        {
            diagnostics.push_back({core::Severity::Error, "ai.host", "comfyui", "", "'" + url + "' is not this computer."});
            return {};
        }
        httplib::Client client(host, port);
        client.set_connection_timeout(2);
        client.set_read_timeout(2);
        const httplib::Result response = client.Get("/object_info/CheckpointLoaderSimple");
        const Json j = response && response->status == 200 ? Json::parse(response->body, nullptr, false) : Json();
        if (!j.is_object() || !j.contains("CheckpointLoaderSimple"))
        {
            diagnostics.push_back({core::Severity::Error, "ai.connect", "comfyui", "",
                                   "ComfyUI is not running at " + url + ". Start ComfyUI (Desktop: port 8000; command line: 8188)."});
            return {};
        }
        const std::vector<std::string> names = Choices(j["CheckpointLoaderSimple"]["input"]["required"]["ckpt_name"]);
        if (names.empty())
            diagnostics.push_back({core::Severity::Error, "ai.model", "comfyui", "",
                                   "ComfyUI has no models yet: put sd_xl_base_1.0.safetensors in its models/checkpoints folder."});
        return names;
    }

    std::string FindComfyUi(std::vector<std::string> &checkpoints)
    {
        for (const char *url : {"http://127.0.0.1:8000", "http://127.0.0.1:8188"})
        {
            std::vector<core::Diagnostic> ignored;
            checkpoints = ListCheckpoints(url, ignored);
            if (!ignored.empty() && ignored[0].code == "ai.connect")
                continue;
            return url;
        }
        return "";
    }
}
