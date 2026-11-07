#include <ComfyUi.hpp>
#include <TextureAssistant.hpp>
#include <TextureTools.hpp>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include <fstream>
#include <thread>

using namespace umbra;
using namespace std::chrono_literals;
using nlohmann::json;

namespace
{
    // A left-to-right ramp: an obvious seam when repeated.
    core::TextureData Ramp(std::uint32_t size)
    {
        core::TextureData t{size, size, {}};
        for (std::uint32_t y = 0; y < size; ++y)
            for (std::uint32_t x = 0; x < size; ++x)
            {
                const auto v = static_cast<std::uint8_t>(x * 255 / (size - 1));
                t.rgba.insert(t.rgba.end(), {v, static_cast<std::uint8_t>(255 - v), 90, 255});
            }
        return t;
    }

    // A stand-in ComfyUI: the prompt text and model name pick the behavior.
    class FakeComfyUi final
    {
    public:
        FakeComfyUi()
        {
            server_.Get("/object_info/CheckpointLoaderSimple", [this](const httplib::Request &, httplib::Response &response) {
                const json names = {"sd_xl_base_1.0.safetensors", "flux1-dev.safetensors"};
                const json input = oldFormat ? json::array({names}) : json::array({"COMBO", {{"options", names}}});
                response.set_content(json{{"CheckpointLoaderSimple", {{"input", {{"required", {{"ckpt_name", input}}}}}}}}.dump(), "application/json");
            });
            server_.Post("/prompt", [this](const httplib::Request &request, httplib::Response &response) {
                std::lock_guard lock(mutex_);
                last = json::parse(request.body);
                const json &graph = last["prompt"];
                const std::string text = graph["2"]["inputs"]["text"];
                if (graph["1"]["inputs"]["ckpt_name"] == "missing.safetensors")
                {
                    response.status = 400;
                    response.set_content(R"({"error":{"message":"Prompt outputs failed validation"},"node_errors":{"1":{"errors":[{"message":"Value not in list","details":"ckpt_name: 'missing.safetensors' not in [...]"}]}}})",
                                         "application/json");
                    return;
                }
                const std::string id = text.find("crash") == 0 ? "p-error" : text.find("slow") == 0 ? "p-slow" : "p-" + std::to_string(++queued);
                polls_[id] = 0;
                response.set_content(json{{"prompt_id", id}, {"number", queued}, {"node_errors", json::object()}}.dump(), "application/json");
            });
            server_.Get(R"(/history/(.+))", [this](const httplib::Request &request, httplib::Response &response) {
                std::lock_guard lock(mutex_);
                const std::string id = request.matches[1];
                // Not finished for the first two polls (and never for "slow").
                if (id == "p-slow" || ++polls_[id] <= 2)
                {
                    response.set_content("{}", "application/json");
                    return;
                }
                json entry = {{"status", {{"status_str", "success"}, {"completed", true}, {"messages", json::array()}}},
                              {"outputs", {{"7", {{"images", {{{"filename", "umbra_temp_00001_.png"}, {"subfolder", ""}, {"type", "temp"}}}}}}}}};
                if (id == "p-error")
                    entry["status"] = {{"status_str", "error"}, {"completed", false},
                                       {"messages", json::array({json::array({"execution_error", {{"exception_message", "CUDA out of memory"}}})})}};
                response.set_content(json{{id, entry}}.dump(), "application/json");
            });
            server_.Get("/view", [](const httplib::Request &request, httplib::Response &response) {
                if (request.get_param_value("type") != "temp")
                    response.status = 404;
                std::vector<std::uint8_t> png;
                assets::EncodePng(Ramp(512), png);
                response.set_content(std::string(png.begin(), png.end()), "image/png");
            });
            server_.Post("/interrupt", [this](const httplib::Request &, httplib::Response &) { ++interrupts; });
            port = server_.bind_to_any_port("127.0.0.1");
            thread_ = std::thread([this] { server_.listen_after_bind(); });
            server_.wait_until_ready();
        }
        ~FakeComfyUi()
        {
            server_.stop();
            thread_.join();
        }

        localai::ImageRequest Request(const std::string &prompt, const std::string &model = "sd_xl_base_1.0.safetensors") const
        {
            localai::ImageRequest request;
            request.host = Url();
            request.model = model;
            request.prompt = prompt;
            request.timeoutSeconds = 20;
            return request;
        }
        [[nodiscard]] std::string Url() const { return "http://127.0.0.1:" + std::to_string(port); }

        int port = 0;
        json last;
        bool oldFormat = false;
        std::atomic<int> interrupts = 0;
        int queued = 0;

    private:
        httplib::Server server_;
        std::thread thread_;
        std::mutex mutex_;
        std::map<std::string, int> polls_;
    };

    bool HasCode(const std::vector<core::Diagnostic> &diagnostics, const std::string &code)
    {
        for (const core::Diagnostic &d : diagnostics)
            if (d.code == code)
                return true;
        return false;
    }

    localai::ImageResult Send(const localai::ImageRequest &request)
    {
        localai::CancelToken cancel;
        return localai::GenerateImage(request, cancel);
    }
}

TEST_CASE("The ComfyUI graph is a plain text-to-image run", "[comfyui]")
{
    localai::ImageRequest request;
    request.prompt = "mossy stone";
    request.negative = "text";
    request.seed = 99;
    const json graph = localai::TextureWorkflow(request);
    CHECK(graph.size() == 7);
    CHECK(graph["1"]["inputs"]["ckpt_name"] == "sd_xl_base_1.0.safetensors");
    CHECK(graph["2"]["inputs"]["text"] == "mossy stone");
    CHECK(graph["3"]["inputs"]["text"] == "text");
    CHECK(graph["4"]["inputs"]["width"] == 1024);
    CHECK(graph["5"]["inputs"]["seed"] == 99);
    CHECK(graph["5"]["inputs"]["model"] == json::array({"1", 0}));
    CHECK(graph["7"]["class_type"] == "PreviewImage");
}

TEST_CASE("A picture is queued, waited for and downloaded", "[comfyui]")
{
    FakeComfyUi comfy;
    const localai::ImageResult result = Send(comfy.Request("mossy stone"));
    INFO((result.diagnostics.empty() ? "" : core::Format(result.diagnostics[0])));
    REQUIRE(result.ok);
    CHECK((result.image.width == 512 && result.image.height == 512));
    CHECK(comfy.last["client_id"] == "umbra");
    CHECK(comfy.last["prompt"]["2"]["inputs"]["text"] == "mossy stone");
}

TEST_CASE("ComfyUI problems are named, never hidden", "[comfyui]")
{
    FakeComfyUi comfy;
    SECTION("model not installed")
    {
        const localai::ImageResult result = Send(comfy.Request("stone", "missing.safetensors"));
        REQUIRE(HasCode(result.diagnostics, "ai.model"));
        CHECK(result.diagnostics[0].message.find("models/checkpoints") != std::string::npos);
    }
    SECTION("the run fails")
    {
        const localai::ImageResult result = Send(comfy.Request("crash please"));
        REQUIRE(HasCode(result.diagnostics, "ai.comfyui"));
        CHECK(result.diagnostics[0].message.find("CUDA out of memory") != std::string::npos);
    }
    SECTION("picture too large")
    {
        localai::ImageRequest request = comfy.Request("stone");
        request.maxBytes = 1000;
        CHECK(HasCode(Send(request).diagnostics, "ai.limit"));
    }
    SECTION("too slow: ComfyUI is told to stop")
    {
        localai::ImageRequest request = comfy.Request("slow stone");
        request.timeoutSeconds = 1;
        CHECK(HasCode(Send(request).diagnostics, "ai.timeout"));
        CHECK(comfy.interrupts == 1);
    }
    SECTION("not this computer (T64)")
    {
        localai::ImageRequest request = comfy.Request("stone");
        request.host = "http://images.example.com:8188";
        CHECK(HasCode(Send(request).diagnostics, "ai.host"));
    }
}

TEST_CASE("Nothing listening means 'ComfyUI is not running'", "[comfyui]")
{
    int closedPort = 0;
    {
        httplib::Server probe;
        closedPort = probe.bind_to_any_port("127.0.0.1");
        probe.stop();
    }
    localai::ImageRequest request;
    request.host = "http://127.0.0.1:" + std::to_string(closedPort);
    const localai::ImageResult result = Send(request);
    REQUIRE(HasCode(result.diagnostics, "ai.connect"));
    CHECK(result.diagnostics[0].message.find("8000") != std::string::npos);
}

TEST_CASE("Cancel stops waiting at once and interrupts ComfyUI", "[comfyui]")
{
    FakeComfyUi comfy;
    localai::CancelToken cancel;
    std::thread canceller([&] {
        std::this_thread::sleep_for(300ms);
        cancel.Cancel();
    });
    const auto start = std::chrono::steady_clock::now();
    const localai::ImageResult result = localai::GenerateImage(comfy.Request("slow stone"), cancel);
    canceller.join();
    CHECK(HasCode(result.diagnostics, "ai.cancel"));
    CHECK(std::chrono::steady_clock::now() - start < 3s);
    CHECK(comfy.interrupts == 1);
}

TEST_CASE("Installed checkpoints are listed in both ComfyUI formats", "[comfyui]")
{
    FakeComfyUi comfy;
    std::vector<core::Diagnostic> diagnostics;
    const std::vector<std::string> expected{"sd_xl_base_1.0.safetensors", "flux1-dev.safetensors"};
    CHECK(localai::ListCheckpoints(comfy.Url(), diagnostics) == expected);
    comfy.oldFormat = true;
    CHECK(localai::ListCheckpoints(comfy.Url(), diagnostics) == expected);
    CHECK(diagnostics.empty());
}

TEST_CASE("Variations run one after another in the background", "[comfyui]")
{
    FakeComfyUi comfy;
    localai::ImageJob job;
    const localai::TextureStyle style;
    REQUIRE(job.Start(localai::MakeTextureRequests("stone", style, "sd_xl_base_1.0.safetensors", comfy.Url(), {1, 2, 3})));
    CHECK_FALSE(job.Start({}));
    std::optional<std::vector<localai::ImageResult>> results;
    for (int i = 0; i < 1000 && !results; ++i)
    {
        std::this_thread::sleep_for(10ms);
        results = job.Take();
    }
    REQUIRE(results);
    REQUIRE(results->size() == 3);
    CHECK(job.Finished() == 3);
    CHECK(comfy.last["prompt"]["5"]["inputs"]["seed"] == 3);

    // A failure stops the batch: the other variations would fail the same way.
    REQUIRE(job.Start(localai::MakeTextureRequests("stone", style, "missing.safetensors", comfy.Url(), {1, 2})));
    for (int i = 0; i < 500 && job.Running(); ++i)
        std::this_thread::sleep_for(10ms);
    CHECK(job.Take()->size() == 1);
}

TEST_CASE("A picture becomes a seamless texture and a material with provenance (T61)", "[comfyui][material]")
{
    localai::ImageRequest request;
    request.model = "sd_xl_base_1.0.safetensors";
    request.prompt = "mossy stone, style";
    request.seed = 1234;
    const localai::TextureCandidate candidate = localai::FinishTexture(Ramp(512), request, 256);
    CHECK((candidate.image.width == 256 && candidate.image.height == 256));
    CHECK(candidate.seamBefore > assets::visibleSeam);
    CHECK(candidate.seam < assets::visibleSeam);

    const std::filesystem::path root = std::filesystem::temp_directory_path() / "umbra-tests-texture-save";
    std::filesystem::remove_all(root);
    std::vector<core::Diagnostic> diagnostics;
    const std::string first = localai::SaveTextureMaterial(root, "Mossy Stone!", candidate, 2.5F, diagnostics);
    const std::string second = localai::SaveTextureMaterial(root, "Mossy Stone!", candidate, 2.5F, diagnostics);
    CHECK(first == "materials/mossy_stone.material.json");
    CHECK(second == "materials/mossy_stone_2.material.json"); // never overwrites
    CHECK(diagnostics.empty());
    CHECK_FALSE(std::filesystem::exists(root / "textures/mossy_stone.png.tmp"));

    assets::Material material;
    REQUIRE(assets::LoadMaterial(root / first, material, diagnostics));
    CHECK(assets::CheckMaterialImage(root, material, first, diagnostics));
    CHECK(material.worldProjection);
    CHECK(material.metersPerRepeat == 2.5F);
    REQUIRE(material.generated);
    CHECK(material.generated->seed == 1234);
    CHECK(material.generated->license == "CreativeML OpenRAIL++-M");
    CHECK(material.generated->prompt == "mossy stone, style");
}

TEST_CASE("The project style is added to every prompt", "[comfyui]")
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "umbra-tests-texture-style";
    std::filesystem::remove_all(root);
    CHECK(localai::LoadTextureStyle(root).prompt.find("seamless") != std::string::npos); // default
    std::filesystem::create_directories(root / "materials");
    std::ofstream(root / "materials/style.json") << R"({"prompt":"pixel art","negative":"photo"})";
    const localai::TextureStyle style = localai::LoadTextureStyle(root);
    const auto requests = localai::MakeTextureRequests("grass", style, "m", "http://127.0.0.1:8000", localai::TextureSeeds(2, 10));
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].prompt == "grass, pixel art");
    CHECK(requests[0].negative == "photo");
    CHECK(requests[1].seed == 11);
    CHECK(localai::TextureName("  Old Oak Planks (dark) ") == "old_oak_planks_dark");
    CHECK(localai::TextureName("!!!") == "texture");
}
