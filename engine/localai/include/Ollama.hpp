#pragma once

#include <Diagnostic.hpp>

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace umbra::localai
{
    // A request to a local Ollama server (roadmap P8B.1). Only this computer is allowed:
    // there is no cloud fallback. See LocalAi.md.
    struct ChatMessage final
    {
        std::string role; // "system", "user" or "assistant"
        std::string content;
    };

    struct ChatRequest final
    {
        std::string host = "http://localhost:11434";
        std::string model = "qwen2.5:7b";
        std::vector<ChatMessage> messages;
        nlohmann::json format;         // JSON schema the answer must follow (null = free text)
        double temperature = 0.7;
        bool think = false;            // reasoning models (qwen3.5, ...) answer much faster without it
        int timeoutSeconds = 600;      // the whole answer, model loading included
        std::size_t maxBytes = 64 * 1024; // longer answers are refused, not cut
    };

    struct ChatResult final
    {
        bool ok = false;
        std::string text; // the model's answer
        std::vector<core::Diagnostic> diagnostics;
    };

    // Lets another thread stop a running Chat at once (it closes the connection).
    class CancelToken final
    {
    public:
        void Cancel();
        void Reset() { cancelled_ = false; }
        // Registers how to stop the running request (nullptr clears it); false if already cancelled.
        bool SetStop(std::function<void()> stop);
        [[nodiscard]] bool Cancelled() const noexcept { return cancelled_; }

    private:
        std::atomic<bool> cancelled_ = false;
        std::mutex mutex_;
        std::function<void()> stop_;
    };

    // True for http://localhost, 127.0.0.1 or [::1]; fills host and port.
    bool ParseLocalUrl(const std::string &url, std::string &host, int &port);

    // Names of the models installed in the local Ollama (GET /api/tags). Quick: 2 s at most.
    std::vector<std::string> ListModels(const std::string &url, std::vector<core::Diagnostic> &diagnostics);

    // Blocking: sends the request and streams the answer. `received` counts answer characters.
    ChatResult Chat(const ChatRequest &request, CancelToken &cancel, std::atomic<std::size_t> *received = nullptr);

    // Runs one Chat on a worker thread so the editor never waits. One job at a time;
    // the destructor cancels and joins.
    class ChatJob final
    {
    public:
        ChatJob() = default;
        ChatJob(const ChatJob &) = delete;
        ChatJob &operator=(const ChatJob &) = delete;
        ~ChatJob();

        bool Start(ChatRequest request); // false while a job runs or its result waits to be taken
        void Cancel();
        [[nodiscard]] bool Running() const noexcept { return running_; }
        // The result, once, after the job has finished.
        [[nodiscard]] std::optional<ChatResult> Take();
        [[nodiscard]] std::size_t Received() const noexcept { return received_; }
        [[nodiscard]] float Seconds() const;

    private:
        std::thread thread_;
        CancelToken cancel_;
        std::atomic<bool> running_ = false;
        std::atomic<std::size_t> received_ = 0;
        std::chrono::steady_clock::time_point started_{};
        std::mutex mutex_;
        std::optional<ChatResult> result_;
    };
}
