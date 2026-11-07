#include <ComfyUi.hpp>

#include <TextureTools.hpp>

#include <httplib.h>

namespace umbra::localai
{
    namespace
    {
        using Json = nlohmann::json;

        void Err(ImageResult &result, const std::string &code, const std::string &message)
        {
            result.diagnostics.push_back({core::Severity::Error, code, "comfyui", "", message});
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
        ImageResult result;
        std::string host;
        int port = 0;
        if (!ParseLocalUrl(request.host, host, port))
        {
            Err(result, "ai.host", "'" + request.host + "' is not this computer. Umbra only uses a local ComfyUI; there is no cloud fallback.");
            return result;
        }
        httplib::Client client(host, port);
        client.set_connection_timeout(3);
        client.set_read_timeout(30);
        if (!cancel.SetStop([&client] { client.stop(); }))
        {
            Err(result, "ai.cancel", "Cancelled.");
            return result;
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(request.timeoutSeconds);
        auto finish = [&] {
            cancel.SetStop(nullptr);
            if (cancel.Cancelled())
            {
                httplib::Client(host, port).Post("/interrupt"); // stop ComfyUI working on it
                result.diagnostics = {{core::Severity::Error, "ai.cancel", "comfyui", "", "Cancelled."}};
                result.ok = false;
            }
            return result;
        };

        // 1. Queue the graph.
        const httplib::Result queued = client.Post("/prompt", Json{{"prompt", TextureWorkflow(request)}, {"client_id", "umbra"}}.dump(), "application/json");
        if (!queued)
        {
            Err(result, "ai.connect", "ComfyUI is not running at " + request.host + " (" + httplib::to_string(queued.error()) +
                                          "). Start ComfyUI (Desktop: port 8000; command line: 8188).");
            return finish();
        }
        const Json reply = Json::parse(queued->body, nullptr, false);
        if (queued->status != 200 || !reply.is_object() || !reply.contains("prompt_id"))
        {
            const std::string why = reply.is_object() ? Refusal(reply) : "";
            if (why.find("ckpt_name") != std::string::npos)
                Err(result, "ai.model", "ComfyUI has no model '" + request.model + "'. Put it in ComfyUI's models/checkpoints folder, or pick an installed one.");
            else
                Err(result, "ai.comfyui", "ComfyUI refused the request (HTTP " + std::to_string(queued->status) + "): " + why);
            return finish();
        }
        const std::string id = reply["prompt_id"].get<std::string>();

        // 2. Wait until the history has it, checking twice a second.
        Json entry;
        while (!cancel.Cancelled())
        {
            if (std::chrono::steady_clock::now() > deadline)
            {
                Err(result, "ai.timeout", "No picture within " + std::to_string(request.timeoutSeconds) + " seconds.");
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
            Err(result, "ai.comfyui", "ComfyUI: " + why);
            return finish();
        }

        // 3. Download the picture of the preview node.
        const Json images = entry.value("outputs", Json::object()).value("7", Json::object()).value("images", Json::array());
        if (images.empty())
        {
            Err(result, "ai.answer", "ComfyUI finished without a picture.");
            return finish();
        }
        const httplib::Params params{{"filename", images[0].value("filename", "")}, {"subfolder", images[0].value("subfolder", "")},
                                     {"type", images[0].value("type", "temp")}};
        std::string bytes;
        const httplib::Result picture = client.Get("/view", params, httplib::Headers{}, [&](const char *data, std::size_t size) {
            bytes.append(data, size);
            return bytes.size() <= request.maxBytes && !cancel.Cancelled();
        });
        if (cancel.Cancelled())
            return finish();
        if (bytes.size() > request.maxBytes)
        {
            Err(result, "ai.limit", "The picture was larger than " + std::to_string(request.maxBytes / (1024 * 1024)) + " MiB and was refused.");
            return finish();
        }
        std::string error;
        if (!picture || picture->status != 200 || !assets::DecodeImage({bytes.begin(), bytes.end()}, result.image, error))
        {
            Err(result, "ai.answer", "Couldn't read ComfyUI's picture" + (error.empty() ? std::string(".") : ": " + error));
            return finish();
        }
        result.ok = true;
        return finish();
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

    ImageJob::~ImageJob()
    {
        Cancel();
        if (thread_.joinable())
            thread_.join();
    }

    bool ImageJob::Start(std::vector<ImageRequest> requests)
    {
        {
            std::lock_guard lock(mutex_);
            if (running_ || results_)
                return false;
        }
        if (thread_.joinable())
            thread_.join();
        cancel_.Reset();
        finished_ = 0;
        started_ = std::chrono::steady_clock::now();
        running_ = true;
        thread_ = std::thread([this, requests = std::move(requests)] {
            std::vector<ImageResult> results;
            for (const ImageRequest &request : requests)
            {
                results.push_back(GenerateImage(request, cancel_));
                ++finished_;
                if (!results.back().ok)
                    break; // the next ones would fail the same way
            }
            std::lock_guard lock(mutex_);
            results_ = std::move(results);
            running_ = false;
        });
        return true;
    }

    std::optional<std::vector<ImageResult>> ImageJob::Take()
    {
        if (running_)
            return std::nullopt;
        std::lock_guard lock(mutex_);
        return std::exchange(results_, std::nullopt);
    }

    float ImageJob::Seconds() const
    {
        return std::chrono::duration<float>(std::chrono::steady_clock::now() - started_).count();
    }
}
