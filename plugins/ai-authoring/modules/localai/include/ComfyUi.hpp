#pragma once

#include <Ollama.hpp>

#include <MeshData.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace umbra::localai
{
    // Pictures (P8C) and 3D models (P8D) through a local ComfyUI. Like Ollama: this computer only,
    // no cloud fallback. ComfyUI Desktop listens on port 8000, the command-line version on 8188.
    struct ImageRequest final
    {
        std::string host = "http://127.0.0.1:8000";
        std::string model = "sd_xl_base_1.0.safetensors"; // a file in ComfyUI's models/checkpoints
        std::string prompt;
        std::string negative;
        long long seed = 0;
        int width = 1024;
        int height = 1024;
        int steps = 25;
        double cfg = 7.0;
        int timeoutSeconds = 120;                // how long ComfyUI may stop answering; a long run itself has no limit
        std::size_t maxBytes = 32 * 1024 * 1024; // a larger image is refused
    };

    struct ImageResult final
    {
        bool ok = false;
        core::TextureData image;
        std::vector<core::Diagnostic> diagnostics;
    };

    // The ComfyUI graph (API format) for one plain text-to-image run.
    [[nodiscard]] nlohmann::json TextureWorkflow(const ImageRequest &request);

    // Blocking: queues the graph, waits for it, downloads the picture.
    ImageResult GenerateImage(const ImageRequest &request, CancelToken &cancel);

    // --- 3D models (P8D): picture -> the creator's exported image-to-3D workflow -> GLB ---

    // An image-to-3D workflow exported from ComfyUI with "Export (API)" (ModelGen.md). Checks that
    // it is API format, has exactly one LoadImage node and a node that saves a 3D file.
    bool LoadModelWorkflow(const std::filesystem::path &file, nlohmann::json &graph, std::vector<core::Diagnostic> &diagnostics);

    struct ModelRequest final
    {
        std::string host = "http://127.0.0.1:8000";
        nlohmann::json workflow;   // from LoadModelWorkflow
        core::TextureData picture; // what the model should look like
        long long seed = 0;
        int faces = 10000;         // DecimateMesh target (user decision 2026-11-07)
        int textureSize = 1024;    // texture bake and UV resolution
        int timeoutSeconds = 120;  // how long ComfyUI may stop answering; a long run itself has no limit
        std::size_t maxBytes = 256 * 1024 * 1024;
        std::string resumeId; // a ComfyUI job queued earlier (editor closed meanwhile): wait for it, queue nothing
        std::function<void(const std::string &)> queued; // told the job id once ComfyUI has it (worker thread)
    };

    struct ModelResult final
    {
        bool ok = false;
        std::vector<std::uint8_t> glb;
        nlohmann::json graph; // what ran (PrepareModelWorkflow), for the provenance
        std::vector<core::Diagnostic> diagnostics;
    };

    // The workflow with this run's picture name, seed, face count and texture size filled in, reduced to
    // what the 3D save node needs: switches resolved, previews and the unused model branch dropped.
    [[nodiscard]] nlohmann::json PrepareModelWorkflow(const ModelRequest &request, const std::string &uploadedPicture);

    // Blocking: uploads the picture, runs the workflow, downloads the GLB its save node wrote.
    ModelResult GenerateModel(const ModelRequest &request, CancelToken &cancel);

    // --- Music and sound effects (S4): ACE-Step 1.5 or Stable Audio 3, both native ComfyUI nodes ---

    struct AudioRequest final
    {
        std::string host = "http://127.0.0.1:8000";
        std::string model = "ace_step_1.5_turbo_aio.safetensors"; // "ace" in the name: ACE-Step; else Stable Audio 3
        std::string textEncoder = "t5gemma_b_b_ul2.safetensors";   // Stable Audio 3 only (models/text_encoders)
        std::string prompt;  // style tags / description, with the project's style already added
        float seconds = 30.0F;
        long long seed = 0;
        int bpm = 100;                  // ACE-Step only
        std::string keyscale = "D minor"; // ACE-Step only
        int timeoutSeconds = 120;
        std::size_t maxBytes = 128 * 1024 * 1024;
    };

    struct AudioResult final
    {
        bool ok = false;
        std::vector<std::uint8_t> file; // what ComfyUI saved (FLAC)
        AudioRequest request;
        std::vector<core::Diagnostic> diagnostics;
    };

    // The graph for one run (SoundGen.md): checkpoint -> text -> sampler -> VAE -> PreviewAudio.
    [[nodiscard]] nlohmann::json AudioWorkflow(const AudioRequest &request);
    // Blocking: queues the graph, waits, downloads the FLAC file it made.
    AudioResult GenerateAudio(const AudioRequest &request, CancelToken &cancel);

    // --- Discovery ---

    // Checkpoint files ComfyUI can load (GET /object_info/CheckpointLoaderSimple). 2 s at most.
    std::vector<std::string> ListCheckpoints(const std::string &url, std::vector<core::Diagnostic> &diagnostics);
    // Tries the Desktop port, then the command-line port; the address that answered, or "".
    [[nodiscard]] std::string FindComfyUi(std::vector<std::string> &checkpoints);

    // Runs requests one after another on a worker thread (variations), stopping at the first failure.
    // Same rules as ChatJob: one batch at a time, results taken once, destructor cancels and joins.
    template <class Request, class Result, Result (*Run)(const Request &, CancelToken &)>
    class BatchJob final
    {
    public:
        BatchJob() = default;
        BatchJob(const BatchJob &) = delete;
        BatchJob &operator=(const BatchJob &) = delete;
        ~BatchJob()
        {
            Cancel();
            if (thread_.joinable())
                thread_.join();
        }

        bool Start(std::vector<Request> requests)
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
                std::vector<Result> results;
                for (const Request &request : requests)
                {
                    results.push_back(Run(request, cancel_));
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
        void Cancel() { cancel_.Cancel(); }
        void Leave() { cancel_.Leave(); } // stop waiting; ComfyUI finishes the work anyway
        [[nodiscard]] bool Running() const noexcept { return running_; }
        [[nodiscard]] std::optional<std::vector<Result>> Take()
        {
            if (running_)
                return std::nullopt;
            std::lock_guard lock(mutex_);
            return std::exchange(results_, std::nullopt);
        }
        [[nodiscard]] std::size_t Finished() const noexcept { return finished_; }
        [[nodiscard]] float Seconds() const { return std::chrono::duration<float>(std::chrono::steady_clock::now() - started_).count(); }

    private:
        std::thread thread_;
        CancelToken cancel_;
        std::atomic<bool> running_ = false;
        std::atomic<std::size_t> finished_ = 0;
        std::chrono::steady_clock::time_point started_{};
        std::mutex mutex_;
        std::optional<std::vector<Result>> results_;
    };

    using ImageJob = BatchJob<ImageRequest, ImageResult, GenerateImage>;
    using ModelJob = BatchJob<ModelRequest, ModelResult, GenerateModel>;
    using AudioJob = BatchJob<AudioRequest, AudioResult, GenerateAudio>;
}
