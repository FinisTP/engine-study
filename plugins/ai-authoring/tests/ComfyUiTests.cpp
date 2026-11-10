#include <AnimationGen.hpp>
#include <ComfyUi.hpp>
#include <Generate.hpp>
#include <ModelAssistant.hpp>
#include <TextureAssistant.hpp>
#include <TextureTools.hpp>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include <fstream>
#include <set>
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
            server_.Post("/upload/image", [this](const httplib::Request &request, httplib::Response &response) {
                std::lock_guard lock(mutex_);
                if (!request.form.has_file("image") || request.form.get_field("type") != "input")
                {
                    response.status = 400;
                    return;
                }
                uploaded = request.form.get_file("image").content;
                uploadedFolder = request.form.has_field("subfolder") ? request.form.get_field("subfolder") : "";
                response.set_content(json{{"name", request.form.get_file("image").filename}, {"subfolder", uploadedFolder}, {"type", "input"}}.dump(),
                                     "application/json");
            });
            server_.Post("/prompt", [this](const httplib::Request &request, httplib::Response &response) {
                std::lock_guard lock(mutex_);
                last = json::parse(request.body);
                const json &graph = last["prompt"];
                // The rigging and motion stand-ins (S8): finished after two polls like the others.
                for (const auto &[node, kind] : {std::pair{"UniRigLoadMesh", "r-1"}, std::pair{"HYMotionGenerate", "h-1"}})
                    for (const auto &[id, n] : graph.items())
                        if (n["class_type"] == node)
                        {
                            polls_[kind] = 0;
                            response.set_content(json{{"prompt_id", kind}, {"number", 1}, {"node_errors", json::object()}}.dump(), "application/json");
                            return;
                        }
                if (graph.contains("40")) // the image-to-3D workflow
                {
                    if (graph["40"]["inputs"]["unet_name"] == "missing.safetensors")
                    {
                        response.status = 400;
                        response.set_content(R"({"error":{"message":"Prompt outputs failed validation"},"node_errors":{"40":{"errors":[{"message":"Value not in list","details":"unet_name: 'missing.safetensors' not in []"}]}}})",
                                             "application/json");
                        return;
                    }
                    polls_["m-1"] = 0;
                    modelGraph = graph;
                    response.set_content(R"({"prompt_id":"m-1","number":1,"node_errors":{}})", "application/json");
                    return;
                }
                const std::string text = graph["2"]["inputs"]["text"];
                if (graph["1"]["inputs"]["ckpt_name"] == "missing.safetensors")
                {
                    response.status = 400;
                    response.set_content(R"({"error":{"message":"Prompt outputs failed validation"},"node_errors":{"1":{"errors":[{"message":"Value not in list","details":"ckpt_name: 'missing.safetensors' not in [...]"}]}}})",
                                         "application/json");
                    return;
                }
                const std::string id = text.find("crash") == 0  ? "p-error"
                                       : text.find("slow") == 0 ? "p-slow"
                                       : text.find("lost") == 0 ? "p-lost"
                                                                : "p-" + std::to_string(++queued);
                polls_[id] = 0;
                response.set_content(json{{"prompt_id", id}, {"number", queued}, {"node_errors", json::object()}}.dump(), "application/json");
            });
            server_.Get(R"(/history/(.+))", [this](const httplib::Request &request, httplib::Response &response) {
                std::lock_guard lock(mutex_);
                const std::string id = request.matches[1];
                // Not finished for the first two polls (and never for "slow" or "lost").
                if (id == "p-slow" || id == "p-lost" || ++polls_[id] <= 2)
                {
                    response.set_content("{}", "application/json");
                    return;
                }
                json entry = {{"status", {{"status_str", "success"}, {"completed", true}, {"messages", json::array()}}},
                              {"outputs", {{"7", {{"images", {{{"filename", "umbra_temp_00001_.png"}, {"subfolder", ""}, {"type", "temp"}}}}}}}}};
                if (id == "m-1") // a preview GLB listed first, like the template's untextured voxel preview
                    entry["outputs"] = {{"246", {{"3d", {{{"filename", "preview3d_white.glb"}, {"subfolder", ""}, {"type", "temp"}}}}}},
                                        {"322", {{"3d", {{{"filename", "ComfyUI_00001_.glb"}, {"subfolder", "3d"}, {"type", "output"}}}}, {"camera_info", {nullptr}}}}};
                if (id == "r-1") // a node that reports a path on disk, as some 3D packs do
                    entry["outputs"] = {{"5", {{"result", {"C:\\ComfyUI\\output\\rigged\\character_00001_.glb"}}}}};
                if (id == "h-1") // an FBX and a GLB of the same motion: the GLB is taken
                    entry["outputs"] = {{"5", {{"3d", {{{"filename", "motion_00001_.fbx"}, {"subfolder", "hymotion"}, {"type", "output"}},
                                                       {{"filename", "motion_00001_.glb"}, {"subfolder", "hymotion"}, {"type", "output"}}}}}}};
                if (id == "p-error")
                    entry["status"] = {{"status_str", "error"}, {"completed", false},
                                       {"messages", json::array({json::array({"execution_error", {{"exception_message", "CUDA out of memory"}}})})}};
                response.set_content(json{{id, entry}}.dump(), "application/json");
            });
            server_.Get("/queue", [this](const httplib::Request &, httplib::Response &response) {
                // Unfinished jobs are running; "lost" is a job a restarted ComfyUI forgot.
                std::lock_guard lock(mutex_);
                json running = json::array();
                for (const auto &[id, polls] : polls_)
                    if (id != "p-lost" && (id == "p-slow" || polls <= 2))
                        running.push_back(json::array({1, id, json::object(), json::object(), json::array()}));
                response.set_content(json{{"queue_running", running}, {"queue_pending", json::array()}}.dump(), "application/json");
            });
            server_.Get("/view", [this](const httplib::Request &request, httplib::Response &response) {
                viewed = request.get_param_value("subfolder") + "/" + request.get_param_value("filename");
                if (request.get_param_value("filename").starts_with("preview"))
                {
                    response.set_content("not the finished model", "model/gltf-binary");
                    return;
                }
                if (request.get_param_value("filename").ends_with(".glb"))
                {
                    // A real model from the repository stands in for TRELLIS.2's output.
                    std::ifstream in(std::filesystem::path(UMBRA_TEST_ASSET_DIRECTORY) / "models/wolf.glb", std::ios::binary);
                    response.set_content(std::string(std::istreambuf_iterator<char>(in), {}), "model/gltf-binary");
                    return;
                }
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
        json last, modelGraph;
        std::string uploaded, uploadedFolder, viewed;
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
    SECTION("ComfyUI forgot the job (restarted)")
    {
        const localai::ImageResult result = Send(comfy.Request("lost stone"));
        REQUIRE(HasCode(result.diagnostics, "ai.lost"));
        CHECK(comfy.interrupts == 0);
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

TEST_CASE("A long run has no time limit while ComfyUI still has it; leaving does not stop ComfyUI", "[comfyui]")
{
    FakeComfyUi comfy;
    localai::CancelToken cancel;
    std::thread leaver([&] {
        std::this_thread::sleep_for(4s); // past the 1 s answer limit and two queue checks
        cancel.Leave();
    });
    localai::ImageRequest request = comfy.Request("slow stone");
    request.timeoutSeconds = 1;
    const localai::ImageResult result = localai::GenerateImage(request, cancel);
    leaver.join();
    CHECK(HasCode(result.diagnostics, "ai.cancel")); // still waiting when left, not timed out or lost
    CHECK(result.diagnostics.size() == 1);
    CHECK(comfy.interrupts == 0); // ComfyUI keeps working (the editor closing)
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

// ---------------- 3D models (P8D) ----------------

namespace
{
    const std::filesystem::path testData = std::filesystem::path(UMBRA_TEST_LOCALAI_DATA);

    std::filesystem::path WriteTemp(const std::string &name, const std::string &text)
    {
        const auto file = std::filesystem::temp_directory_path() / ("umbra-tests-" + name);
        std::ofstream(file) << text;
        return file;
    }
}

TEST_CASE("An exported image-to-3D workflow is checked before use", "[comfyui][model]")
{
    json graph;
    std::vector<core::Diagnostic> diagnostics;
    CHECK(localai::LoadModelWorkflow(testData / "image-to-3d.api.json", graph, diagnostics));
    CHECK(diagnostics.empty());

    SECTION("missing file")
    {
        CHECK_FALSE(localai::LoadModelWorkflow(testData / "none.json", graph, diagnostics));
        CHECK(HasCode(diagnostics, "workflow.missing"));
    }
    SECTION("UI format instead of API format")
    {
        CHECK_FALSE(localai::LoadModelWorkflow(WriteTemp("ui.json", R"({"nodes":[{"id":1}],"links":[]})"), graph, diagnostics));
        REQUIRE(HasCode(diagnostics, "workflow.format"));
        CHECK(diagnostics.back().message.find("Export (API)") != std::string::npos);
    }
    SECTION("no picture input, or no 3D output")
    {
        CHECK_FALSE(localai::LoadModelWorkflow(WriteTemp("noimage.json", R"({"1":{"class_type":"SaveGLB","inputs":{}}})"), graph, diagnostics));
        CHECK(HasCode(diagnostics, "workflow.image"));
        CHECK_FALSE(localai::LoadModelWorkflow(WriteTemp("nosave.json", R"({"1":{"class_type":"LoadImage","inputs":{"image":"a.png"}}})"), graph, diagnostics));
        CHECK(HasCode(diagnostics, "workflow.output"));
    }
}

TEST_CASE("The workflow gets this run's picture, seeds, face count and texture size", "[comfyui][model]")
{
    localai::ModelRequest request;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(localai::LoadModelWorkflow(testData / "image-to-3d.api.json", request.workflow, diagnostics));
    request.seed = 1000;
    const json graph = localai::PrepareModelWorkflow(request, "umbra_model_input.png");
    CHECK(graph["122"]["inputs"]["image"] == "umbra_model_input.png");
    CHECK(graph["186"]["inputs"]["target_face_count"] == 10000); // decided budget
    CHECK(graph["288"]["inputs"]["value"] == 1024);              // through the PrimitiveInt wired into texture_size and resolution
    CHECK(graph["147"]["inputs"]["texture_size"] == json::array({"288", 0}));
    std::set<long long> seeds;
    for (const char *sampler : {"3", "12", "18", "23"})
        seeds.insert(graph[sampler]["inputs"]["seed"].get<long long>());
    CHECK(seeds == std::set<long long>{1000, 1001, 1002, 1003});
    CHECK(request.workflow["186"]["inputs"]["target_face_count"] == 700000); // the loaded workflow is untouched

    // Only what the save node needs (T71): the switch picks TRELLIS.2; previews and Pixal3D are gone.
    CHECK(graph.contains("322"));
    CHECK(graph.contains("40"));
    for (const char *gone : {"246", "247", "4", "323", "282", "252", "261", "319", "298", "56", "316", "318", "314", "248"})
        CHECK_FALSE(graph.contains(gone));
    CHECK(graph["192"]["inputs"]["image"] == json::array({"122", 0})); // background removal kept (its switch was on)
    request.workflow["316"]["inputs"]["value"] = false;               // the template switched to Pixal3D
    const json pixal = localai::PrepareModelWorkflow(request, "x.png");
    CHECK(pixal.contains("319"));
    CHECK_FALSE(pixal.contains("40"));
}

TEST_CASE("A picture is uploaded, turned into a model and downloaded", "[comfyui][model]")
{
    FakeComfyUi comfy;
    localai::ModelRequest request;
    request.host = comfy.Url();
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(localai::LoadModelWorkflow(testData / "image-to-3d.api.json", request.workflow, diagnostics));
    request.picture = Ramp(64);
    localai::CancelToken cancel;
    const localai::ModelResult result = localai::GenerateModel(request, cancel);
    INFO((result.diagnostics.empty() ? "" : core::Format(result.diagnostics[0])));
    REQUIRE(result.ok);
    CHECK(result.glb.size() == std::filesystem::file_size(std::filesystem::path(UMBRA_TEST_ASSET_DIRECTORY) / "models/wolf.glb"));
    CHECK(comfy.uploaded.substr(1, 3) == "PNG");
    CHECK(comfy.last["prompt"]["122"]["inputs"]["image"] == "umbra_model_input.png");
    CHECK(comfy.last["prompt"]["186"]["inputs"]["target_face_count"] == 10000);
    CHECK_FALSE(comfy.last["prompt"].contains("246")); // previews are not run

    SECTION("an earlier job is fetched by its id, nothing queued again")
    {
        comfy.uploaded.clear();
        comfy.last = nullptr;
        request.resumeId = "m-1";
        const localai::ModelResult resumed = localai::GenerateModel(request, cancel);
        REQUIRE(resumed.ok);
        CHECK(resumed.glb.size() == result.glb.size());
        CHECK(comfy.uploaded.empty());
        CHECK(comfy.last.is_null());
    }
    SECTION("the job id is told once queued")
    {
        std::string id;
        request.queued = [&id](const std::string &queuedId) { id = queuedId; };
        REQUIRE(localai::GenerateModel(request, cancel).ok);
        CHECK(id == "m-1");
    }
    SECTION("a model the workflow needs is missing")
    {
        request.workflow["40"]["inputs"]["unet_name"] = "missing.safetensors";
        const localai::ModelResult missing = localai::GenerateModel(request, cancel);
        REQUIRE(HasCode(missing.diagnostics, "ai.model"));
        CHECK(missing.diagnostics[0].message.find("unet_name") != std::string::npos);
    }
    SECTION("not this computer (T64)")
    {
        request.host = "http://models.example.com:8000";
        CHECK(HasCode(localai::GenerateModel(request, cancel).diagnostics, "ai.host"));
    }
}

TEST_CASE("A generated model is saved with its picture and provenance, and checked", "[comfyui][model]")
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "umbra-tests-model-save";
    std::filesystem::remove_all(root);
    std::ifstream in(std::filesystem::path(UMBRA_TEST_ASSET_DIRECTORY) / "models/wolf.glb", std::ios::binary);
    const std::vector<std::uint8_t> glb((std::istreambuf_iterator<char>(in)), {});
    localai::ModelRequest request;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(localai::LoadModelWorkflow(testData / "image-to-3d.api.json", request.workflow, diagnostics));
    const json workflow = localai::PrepareModelWorkflow(request, "x.png"); // what ran
    const localai::ModelSource source{"a mossy barrel", "image-to-3d.api.json", 7, 10000};

    const std::string first = localai::SaveGeneratedModel(root, "Mossy Barrel", glb, Ramp(64), source, workflow, diagnostics);
    const std::string second = localai::SaveGeneratedModel(root, "Mossy Barrel", glb, Ramp(64), source, workflow, diagnostics);
    CHECK(first == "models/mossy_barrel.glb");
    CHECK(second == "models/mossy_barrel_2.glb");
    CHECK(std::filesystem::exists(root / "models/mossy_barrel.concept.png"));
    std::ifstream provenanceFile(root / "models/mossy_barrel.source.json");
    const json provenance = json::parse(provenanceFile);
    CHECK(provenance["seed"] == 7);
    CHECK(provenance["picture"] == "a mossy barrel");
    int trellis = 0;
    for (const json &model : provenance["models"])
    {
        CHECK(model["file"].get<std::string>().find("pixal3d") == std::string::npos); // the unused branch isn't listed
        if (model["file"].get<std::string>().starts_with("trellis_2"))
        {
            ++trellis;
            CHECK(model["license"] == "MIT (TRELLIS.2)");
            CHECK(model["commercial"] == true);
        }
    }
    CHECK(trellis == 3);

    localai::ModelInfo info;
    REQUIRE(authoring::InspectModel(root / first, info, diagnostics));
    CHECK(info.triangles > 1000);
    CHECK(info.max.y > info.min.y);

    // Bytes the game can't read are not kept.
    std::vector<core::Diagnostic> broken;
    CHECK(localai::SaveGeneratedModel(root, "broken", {1, 2, 3}, Ramp(64), source, workflow, broken).empty());
    CHECK(HasCode(broken, "model.import"));
    CHECK_FALSE(std::filesystem::exists(root / "models/broken.glb"));
    CHECK_FALSE(std::filesystem::exists(root / "models/broken.source.json"));
}

TEST_CASE("A placed model stands on the ground at the asked height with a free id", "[comfyui][model]")
{
    localai::ModelInfo info;
    info.meshes = 1;
    info.min = {-0.5F, -0.5F, -0.5F};
    info.max = {0.5F, 0.5F, 0.5F};
    const nlohmann::ordered_json scene = {{"entities", {{{"id", "barrel"}}}}};
    const auto entity = localai::PlacedModelEntity(scene, "Barrel", "models/barrel.glb", info, {2.0F, 0.0F, -3.0F}, 2.0F);
    CHECK(entity["id"] == "barrel_2");
    CHECK(entity["scale"][0] == 2.0F);
    CHECK(entity["position"][1] == 1.0F); // lowest point (-0.5 x 2) lifted to y = 0
    CHECK(entity["position"][0] == 2.0F);
    CHECK(entity["model"] == "models/barrel.glb");
    // A static box fitted to the model at its scale: 1 m model x 2 = 1 m half size (T73).
    CHECK(entity["components"]["collider"]["shape"] == "box");
    CHECK(entity["components"]["collider"]["motion"] == "static");
    CHECK(entity["components"]["collider"]["halfExtents"] == nlohmann::ordered_json::array({1.0F, 1.0F, 1.0F}));
    info.max.y = 1.5F; // taller than wide: the box follows each axis
    CHECK(authoring::FittedCollider(info, {2.0F, 2.0F, 2.0F})["halfExtents"][1] == 2.0F);
    info.max.y = 0.5F;
    info.meshes = 3;
    CHECK(localai::PlacedModelEntity(scene, "Barrel", "models/barrel.glb", info, {}, 1.0F)["instance"] == "models/barrel.glb");
}

TEST_CASE("Concept pictures of props use the object style", "[comfyui][model]")
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "umbra-tests-object-style";
    std::filesystem::remove_all(root);
    CHECK(localai::LoadObjectStyle(root).prompt.find("plain white background") != std::string::npos);
    std::filesystem::create_directories(root / "materials");
    std::ofstream(root / "materials/style.json") << R"({"prompt":"tiles","objectPrompt":"toy figure","objectNegative":"photo"})";
    CHECK(localai::LoadObjectStyle(root).prompt == "toy figure");
    CHECK(localai::LoadTextureStyle(root).prompt == "tiles");
    CHECK(localai::DefaultModelWorkflow("E:/Projects/EngineStudy/assets") == std::filesystem::path("E:/Projects/EngineStudy/tools/comfyui/image-to-3d.api.json"));
}

TEST_CASE("An exported workflow is found whatever its name", "[comfyui][model]")
{
    const std::filesystem::path project = std::filesystem::temp_directory_path() / "umbra-tests-workflows";
    std::filesystem::remove_all(project);
    std::filesystem::create_directories(project / "tools/comfyui");
    std::ofstream(project / "tools/comfyui/notes.json") << R"({"nodes":[]})"; // a UI workflow: not usable
    std::filesystem::copy_file(testData / "image-to-3d.api.json", project / "tools/comfyui/my_trellis_export.json");
    CHECK(localai::FindModelWorkflows(project / "tools/comfyui") == std::vector<std::filesystem::path>{project / "tools/comfyui/my_trellis_export.json"});
    CHECK(localai::DefaultModelWorkflow(project / "assets") == project / "tools/comfyui/my_trellis_export.json");
    std::filesystem::copy_file(testData / "image-to-3d.api.json", project / "tools/comfyui/image-to-3d.api.json");
    CHECK(localai::DefaultModelWorkflow(project / "assets") == project / "tools/comfyui/image-to-3d.api.json");
}

// ---------------- Rigging and motion (S8) ----------------

TEST_CASE("Exported rigging and motion workflows are checked before use (T88)", "[comfyui][anim]")
{
    json graph;
    std::vector<core::Diagnostic> diagnostics;
    CHECK(localai::LoadAnimWorkflow(testData / "rig.api.json", localai::AnimKind::Rig, graph, diagnostics));
    CHECK(localai::LoadAnimWorkflow(testData / "motion.api.json", localai::AnimKind::Motion, graph, diagnostics));
    CHECK(diagnostics.empty());
    // Each kind refuses the other's workflow: no model file to replace, no text to fill in.
    CHECK_FALSE(localai::LoadAnimWorkflow(testData / "motion.api.json", localai::AnimKind::Rig, graph, diagnostics));
    CHECK(HasCode(diagnostics, "workflow.model"));
    CHECK_FALSE(localai::LoadAnimWorkflow(testData / "rig.api.json", localai::AnimKind::Motion, graph, diagnostics));
    CHECK(HasCode(diagnostics, "workflow.text"));
    CHECK_FALSE(localai::LoadAnimWorkflow(WriteTemp("ui-rig.json", R"({"nodes":[],"links":[]})"), localai::AnimKind::Rig, graph, diagnostics));
    CHECK(HasCode(diagnostics, "workflow.format"));
}

TEST_CASE("A motion workflow gets the description, length, seed and character", "[comfyui][anim]")
{
    localai::AnimRequest request;
    request.kind = localai::AnimKind::Motion;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(localai::LoadAnimWorkflow(testData / "motion.api.json", request.kind, request.workflow, diagnostics));
    request.prompt = "swings a sword overhead";
    request.seconds = 2.5F;
    request.seed = 77;
    const json graph = localai::PrepareAnimWorkflow(request, "3d/hero.fbx");
    CHECK(graph["2"]["inputs"]["text"] == "swings a sword overhead");
    CHECK(graph["4"]["inputs"]["duration"] == 2.5);
    CHECK(graph["4"]["inputs"]["seed"] == 77);
    CHECK(graph["5"]["inputs"]["custom_fbx_path"] == "3d/hero.fbx");
}

TEST_CASE("A model is rigged and a motion made through ComfyUI, saved and checked (T88)", "[comfyui][anim]")
{
    FakeComfyUi comfy;
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "umbra-tests-anim" / "assets";
    std::filesystem::remove_all(root.parent_path());
    std::filesystem::create_directories(root / "models");
    std::filesystem::copy_file(std::filesystem::path(UMBRA_TEST_ASSET_DIRECTORY) / "models/elder.glb", root / "models/elder.glb");
    localai::GenerateOptions options;
    options.assets = assets::AssetFolders(root);
    options.host = comfy.Url();
    options.seed = 5;

    SECTION("rig: the model goes into the loader's folder; the rigged GLB is saved with its source")
    {
        options.source = "models/elder.glb";
        options.workflow = testData / "rig.api.json";
        const localai::GenerateResult result = localai::CreateRig(options);
        INFO((result.diagnostics.empty() ? "" : core::Format(result.diagnostics[0])));
        REQUIRE(result.made.size() == 1);
        CHECK(result.made[0].path == "models/elder_rigged.glb");
        CHECK(result.made[0].info["joints"].get<int>() > 0);
        CHECK(comfy.uploadedFolder == "3d");
        CHECK(comfy.last["prompt"]["1"]["inputs"]["mesh_path"] == "3d/elder.glb");
        CHECK(comfy.viewed == "rigged/character_00001_.glb");
        std::ifstream sourceFile(root / "models/elder_rigged.source.json");
        CHECK(json::parse(sourceFile)["source"] == "models/elder.glb");
    }
    SECTION("motion: text in, an animation file out")
    {
        options.idea = "waves hello";
        options.seconds = 3.0F;
        options.workflow = testData / "motion.api.json";
        const localai::GenerateResult result = localai::CreateMotion(options);
        INFO((result.diagnostics.empty() ? "" : core::Format(result.diagnostics[0])));
        REQUIRE(result.made.size() == 1);
        CHECK(result.made[0].path == "animations/waves_hello.glb");
        CHECK_FALSE(result.made[0].info["clips"].empty());
        CHECK(comfy.viewed == "hymotion/motion_00001_.glb");
        CHECK(comfy.last["prompt"]["2"]["inputs"]["text"] == "waves hello");
    }
    SECTION("a result without a skeleton is not kept")
    {
        std::vector<core::Diagnostic> diagnostics;
        std::ifstream in(root / "models/elder.glb", std::ios::binary);
        const std::vector<std::uint8_t> plain((std::istreambuf_iterator<char>(in)), {});
        CHECK(localai::SaveAnimResult(root, localai::AnimKind::Rig, "plain", plain, {}, diagnostics).empty());
        CHECK(HasCode(diagnostics, "model.rig"));
        CHECK_FALSE(std::filesystem::exists(root / "models/plain.glb"));
    }
}

TEST_CASE("An FBX result needs Blender; without it the error says what to do", "[comfyui][anim]")
{
#ifdef _WIN32
    _putenv_s("UMBRA_BLENDER", "C:/no/such/blender.exe");
#else
    setenv("UMBRA_BLENDER", "/no/such/blender", 1);
#endif
    std::vector<std::uint8_t> glb;
    std::vector<core::Diagnostic> diagnostics;
    CHECK_FALSE(localai::FbxToGlb({'K', 'a', 'y'}, glb, diagnostics));
    REQUIRE(HasCode(diagnostics, "ai.convert"));
    CHECK(diagnostics[0].message.find("UMBRA_BLENDER") != std::string::npos);
#ifdef _WIN32
    _putenv_s("UMBRA_BLENDER", "");
#else
    unsetenv("UMBRA_BLENDER");
#endif
}
