#pragma once

#include <Ollama.hpp>

#include <MeshData.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace umbra::localai
{
    // Image generation through a local ComfyUI (roadmap P8C.2). Like Ollama: this computer only,
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
        int timeoutSeconds = 600;                   // model loading included
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

    // Checkpoint files ComfyUI can load (GET /object_info/CheckpointLoaderSimple). 2 s at most.
    std::vector<std::string> ListCheckpoints(const std::string &url, std::vector<core::Diagnostic> &diagnostics);
    // Tries the Desktop port, then the command-line port; the address that answered, or "".
    [[nodiscard]] std::string FindComfyUi(std::vector<std::string> &checkpoints);

    // Runs several requests one after another on a worker thread (variations of one texture).
    // Same rules as ChatJob: one batch at a time, result taken once, destructor cancels and joins.
    class ImageJob final
    {
    public:
        ImageJob() = default;
        ImageJob(const ImageJob &) = delete;
        ImageJob &operator=(const ImageJob &) = delete;
        ~ImageJob();

        bool Start(std::vector<ImageRequest> requests);
        void Cancel() { cancel_.Cancel(); }
        [[nodiscard]] bool Running() const noexcept { return running_; }
        [[nodiscard]] std::optional<std::vector<ImageResult>> Take();
        [[nodiscard]] std::size_t Finished() const noexcept { return finished_; }
        [[nodiscard]] float Seconds() const;

    private:
        std::thread thread_;
        CancelToken cancel_;
        std::atomic<bool> running_ = false;
        std::atomic<std::size_t> finished_ = 0;
        std::chrono::steady_clock::time_point started_{};
        std::mutex mutex_;
        std::optional<std::vector<ImageResult>> results_;
    };
}
