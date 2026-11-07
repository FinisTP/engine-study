#include <Ollama.hpp>

#include <httplib.h>

namespace umbra::localai
{
    namespace
    {
        using Json = nlohmann::json;

        void Err(ChatResult &result, const std::string &code, const std::string &message)
        {
            result.diagnostics.push_back({core::Severity::Error, code, "ollama", "", message});
        }

        // Ollama reports problems as {"error": "..."}; returns "" if `text` isn't one.
        std::string ErrorIn(const std::string &text)
        {
            const Json j = Json::parse(text, nullptr, false);
            return j.is_object() && j.contains("error") ? (j["error"].is_string() ? j["error"].get<std::string>() : j["error"].dump()) : "";
        }
    }

    void CancelToken::Cancel()
    {
        cancelled_ = true;
        std::lock_guard lock(mutex_);
        if (stop_)
            stop_(); // closes the socket, so a waiting read returns now
    }

    bool ParseLocalUrl(const std::string &url, std::string &host, int &port)
    {
        const std::string scheme = "http://";
        if (!url.starts_with(scheme))
            return false;
        std::string rest = url.substr(scheme.size());
        if (const std::size_t slash = rest.find('/'); slash != std::string::npos)
        {
            if (rest.substr(slash) != "/")
                return false;
            rest.resize(slash);
        }
        // 1. Host: "[::1]" or everything before ':'.
        if (rest.starts_with("[::1]"))
        {
            host = "::1";
            rest = rest.substr(5);
        }
        else
        {
            const std::size_t colon = rest.find(':');
            host = rest.substr(0, colon);
            rest = colon == std::string::npos ? "" : rest.substr(colon);
        }
        // 2. Optional ":port".
        port = 11434;
        if (!rest.empty())
        {
            if (rest.size() < 2 || rest.size() > 6 || rest[0] != ':' || rest.find_first_not_of("0123456789", 1) != std::string::npos)
                return false;
            port = std::stoi(rest.substr(1));
        }
        return port > 0 && port < 65536 && (host == "localhost" || host == "127.0.0.1" || host == "::1");
    }

    ChatResult Chat(const ChatRequest &request, CancelToken &cancel, std::atomic<std::size_t> *received)
    {
        ChatResult result;
        std::string host;
        int port = 0;
        if (!ParseLocalUrl(request.host, host, port))
        {
            Err(result, "ai.host", "'" + request.host + "' is not this computer. Umbra only uses a local Ollama "
                                   "(e.g. http://localhost:11434); there is no cloud fallback.");
            return result;
        }

        httplib::Client client(host, port);
        client.set_connection_timeout(3);
        client.set_read_timeout(request.timeoutSeconds);
        {
            std::lock_guard lock(cancel.mutex_);
            if (cancel.Cancelled())
            {
                Err(result, "ai.cancel", "Cancelled.");
                return result;
            }
            cancel.stop_ = [&client] { client.stop(); };
        }

        // 1. The request: streamed, so progress shows and cancel works mid-answer.
        Json messages = Json::array();
        for (const ChatMessage &m : request.messages)
            messages.push_back({{"role", m.role}, {"content", m.content}});
        Json body = {{"model", request.model}, {"messages", messages}, {"stream", true}, {"options", {{"temperature", request.temperature}}}};
        if (!request.format.is_null())
            body["format"] = request.format;

        // 2. The answer arrives as one JSON object per line: {"message":{"content":"..."},"done":false}.
        std::string buffer, text, error;
        bool done = false, malformed = false, tooLong = false, timedOut = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(request.timeoutSeconds);
        auto receive = [&](const char *data, std::size_t size) {
            buffer.append(data, size);
            for (std::size_t end; (end = buffer.find('\n')) != std::string::npos;)
            {
                const std::string line = buffer.substr(0, end);
                buffer.erase(0, end + 1);
                if (line.empty())
                    continue;
                const Json j = Json::parse(line, nullptr, false);
                if (!j.is_object())
                {
                    malformed = true;
                    return false;
                }
                if (!(error = ErrorIn(line)).empty())
                    return false;
                if (j.contains("message") && j["message"].contains("content") && j["message"]["content"].is_string())
                    text += j["message"]["content"].get<std::string>();
                done = done || j.value("done", false);
                if (received != nullptr)
                    *received = text.size();
            }
            tooLong = text.size() > request.maxBytes || buffer.size() > request.maxBytes;
            timedOut = std::chrono::steady_clock::now() > deadline;
            return !tooLong && !timedOut && !cancel.Cancelled();
        };
        const httplib::Result response = client.Post("/api/chat", httplib::Headers{}, body.dump(), "application/json", receive);
        {
            std::lock_guard lock(cancel.mutex_);
            cancel.stop_ = nullptr;
        }
        if (error.empty() && !buffer.empty())
            error = ErrorIn(buffer); // an error reply without a final newline
        if (error.empty() && response && response->status != 200)
            error = ErrorIn(response->body);

        // 3. Name the problem so a creator can fix it.
        const std::string model = "'" + request.model + "'";
        if (cancel.Cancelled())
            Err(result, "ai.cancel", "Cancelled.");
        else if (tooLong)
            Err(result, "ai.limit", "The answer was longer than " + std::to_string(request.maxBytes / 1024) + " KiB and was refused.");
        else if (timedOut || (!response && response.error() == httplib::Error::Read))
            Err(result, "ai.timeout", "No complete answer within " + std::to_string(request.timeoutSeconds) + " seconds.");
        else if (error.find("not found") != std::string::npos || (response && response->status == 404))
            Err(result, "ai.model", "The model " + model + " is not installed. In a terminal run: ollama pull " + request.model);
        else if (!error.empty())
            Err(result, "ai.ollama", "Ollama: " + error);
        else if (malformed)
            Err(result, "ai.answer", "Ollama's answer stream was malformed.");
        else if (!response && response.error() == httplib::Error::Connection)
            Err(result, "ai.connect", "Ollama is not running at " + request.host + ". Start Ollama (install it from ollama.com), then try again.");
        else if (!response)
            Err(result, "ai.http", std::string("Connection problem: ") + httplib::to_string(response.error()) + ".");
        else if (response->status != 200)
            Err(result, "ai.http", "Ollama answered HTTP " + std::to_string(response->status) + ".");
        else if (!done)
            Err(result, "ai.answer", "Ollama's answer stream was cut off.");
        else
        {
            result.ok = true;
            result.text = std::move(text);
        }
        return result;
    }

    ChatJob::~ChatJob()
    {
        Cancel();
        if (thread_.joinable())
            thread_.join();
    }

    bool ChatJob::Start(ChatRequest request)
    {
        if (running_)
            return false;
        {
            std::lock_guard lock(mutex_);
            if (result_)
                return false; // the last result hasn't been taken yet
        }
        if (thread_.joinable())
            thread_.join();
        cancel_.Reset();
        received_ = 0;
        started_ = std::chrono::steady_clock::now();
        running_ = true;
        thread_ = std::thread([this, request = std::move(request)] {
            ChatResult result = Chat(request, cancel_, &received_);
            std::lock_guard lock(mutex_);
            result_ = std::move(result);
            running_ = false;
        });
        return true;
    }

    void ChatJob::Cancel()
    {
        cancel_.Cancel();
    }

    std::optional<ChatResult> ChatJob::Take()
    {
        if (running_)
            return std::nullopt;
        std::lock_guard lock(mutex_);
        return std::exchange(result_, std::nullopt);
    }

    float ChatJob::Seconds() const
    {
        return std::chrono::duration<float>(std::chrono::steady_clock::now() - started_).count();
    }
}
