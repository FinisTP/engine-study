#include <Ollama.hpp>
#include <QuestAssistant.hpp>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include <fstream>
#include <sstream>
#include <thread>

using namespace umbra;
using namespace std::chrono_literals;
using nlohmann::json;

namespace
{
    std::string GoodDraft()
    {
        std::ifstream in(std::filesystem::path(UMBRA_TEST_DATA_DIRECTORY) / "good.draft.json");
        std::stringstream text;
        text << in.rdbuf();
        return json::parse(text.str()).dump(); // one line, like a model's answer
    }

    // A stand-in Ollama: the requested model name picks the behavior.
    class FakeOllama final
    {
    public:
        FakeOllama()
        {
            server_.Post("/api/chat", [this](const httplib::Request &request, httplib::Response &response) {
                last = json::parse(request.body);
                const std::string model = last["model"];
                if (model == "missing" || model == "broken")
                {
                    response.status = model == "missing" ? 404 : 500;
                    response.set_content(model == "missing" ? R"({"error":"model \"missing\" not found, try pulling it first"})"
                                                            : R"({"error":"out of memory"})",
                                         "application/json");
                    return;
                }
                response.set_chunked_content_provider("application/x-ndjson", [model](std::size_t, httplib::DataSink &sink) {
                    auto line = [&](const json &j) {
                        const std::string text = j.dump() + "\n";
                        return sink.write(text.data(), text.size());
                    };
                    if (model == "good")
                    {
                        // The answer in small pieces, like a model writing token by token.
                        const std::string answer = GoodDraft();
                        for (std::size_t i = 0; i < answer.size(); i += 40)
                            line({{"message", {{"role", "assistant"}, {"content", answer.substr(i, 40)}}}, {"done", false}});
                        line({{"message", {{"role", "assistant"}, {"content", ""}}}, {"done", true}});
                    }
                    else if (model == "garbage")
                        sink.write("this is not json\n", 17);
                    else if (model == "cut")
                        line({{"message", {{"content", "{\"quest\":"}}}, {"done", false}});
                    else if (model == "huge")
                        for (int i = 0; i < 200 && sink.is_writable(); ++i)
                            line({{"message", {{"content", std::string(1000, 'x')}}}, {"done", false}});
                    else if (model == "slow")
                        for (int i = 0; i < 150 && sink.is_writable(); ++i)
                            std::this_thread::sleep_for(20ms); // "loading the model"
                    sink.done();
                    return true;
                });
            });
            port = server_.bind_to_any_port("127.0.0.1");
            thread_ = std::thread([this] { server_.listen_after_bind(); });
            server_.wait_until_ready();
        }
        ~FakeOllama()
        {
            server_.stop();
            thread_.join();
        }

        localai::ChatRequest Request(const std::string &model) const
        {
            localai::ChatRequest request;
            request.host = "http://127.0.0.1:" + std::to_string(port);
            request.model = model;
            request.messages = {{"user", "hello"}};
            request.timeoutSeconds = 10;
            return request;
        }

        int port = 0;
        json last;

    private:
        httplib::Server server_;
        std::thread thread_;
    };

    bool HasCode(const localai::ChatResult &result, const std::string &code)
    {
        for (const core::Diagnostic &d : result.diagnostics)
            if (d.code == code)
                return true;
        return false;
    }

    localai::ChatResult Send(const localai::ChatRequest &request)
    {
        localai::CancelToken cancel;
        return localai::Chat(request, cancel);
    }
}

TEST_CASE("Only this computer is accepted as the Ollama address (T58)", "[localai]")
{
    std::string host;
    int port = 0;
    CHECK(localai::ParseLocalUrl("http://localhost:11434", host, port));
    CHECK((host == "localhost" && port == 11434));
    CHECK(localai::ParseLocalUrl("http://127.0.0.1", host, port));
    CHECK(port == 11434);
    CHECK(localai::ParseLocalUrl("http://[::1]:8080/", host, port));
    CHECK((host == "::1" && port == 8080));
    CHECK_FALSE(localai::ParseLocalUrl("https://localhost:11434", host, port));
    CHECK_FALSE(localai::ParseLocalUrl("http://api.example.com", host, port));
    CHECK_FALSE(localai::ParseLocalUrl("http://localhost.example.com:11434", host, port));
    CHECK_FALSE(localai::ParseLocalUrl("http://localhost:11434/api/elsewhere", host, port));
    CHECK_FALSE(localai::ParseLocalUrl("http://localhost:99999", host, port));

    localai::ChatRequest request;
    request.host = "http://api.example.com";
    const localai::ChatResult result = Send(request);
    CHECK_FALSE(result.ok);
    CHECK(HasCode(result, "ai.host"));
}

TEST_CASE("A streamed answer arrives whole, with the schema in the request", "[localai]")
{
    FakeOllama ollama;
    localai::ChatRequest request = ollama.Request("good");
    request.format = {{"type", "object"}};
    localai::CancelToken cancel;
    std::atomic<std::size_t> received = 0;
    const localai::ChatResult result = localai::Chat(request, cancel, &received);
    REQUIRE(result.ok);
    CHECK(result.text == GoodDraft());
    CHECK(received == result.text.size());
    CHECK(ollama.last["stream"] == true);
    CHECK(ollama.last["format"]["type"] == "object");
    CHECK(ollama.last["messages"][0]["content"] == "hello");
}

TEST_CASE("Provider failures are named, never hidden (P8B.1)", "[localai]")
{
    FakeOllama ollama;
    SECTION("model not installed")
    {
        const localai::ChatResult result = Send(ollama.Request("missing"));
        REQUIRE(HasCode(result, "ai.model"));
        CHECK(result.diagnostics[0].message.find("ollama pull missing") != std::string::npos);
    }
    SECTION("server error")
    {
        CHECK(HasCode(Send(ollama.Request("broken")), "ai.ollama"));
    }
    SECTION("malformed stream")
    {
        CHECK(HasCode(Send(ollama.Request("garbage")), "ai.answer"));
    }
    SECTION("stream ends without done")
    {
        CHECK(HasCode(Send(ollama.Request("cut")), "ai.answer"));
    }
    SECTION("answer too long")
    {
        localai::ChatRequest request = ollama.Request("huge");
        request.maxBytes = 16 * 1024;
        const localai::ChatResult result = Send(request);
        CHECK_FALSE(result.ok);
        CHECK(result.text.empty());
        CHECK(HasCode(result, "ai.limit"));
    }
    SECTION("too slow")
    {
        localai::ChatRequest request = ollama.Request("slow");
        request.timeoutSeconds = 1;
        CHECK(HasCode(Send(request), "ai.timeout"));
    }
}

TEST_CASE("Nothing listening means 'Ollama is not running'", "[localai]")
{
    int closedPort = 0;
    {
        httplib::Server probe;
        closedPort = probe.bind_to_any_port("127.0.0.1");
        probe.stop(); // closes the socket: nothing listens on that port any more
    }
    localai::ChatRequest request;
    request.host = "http://127.0.0.1:" + std::to_string(closedPort);
    request.timeoutSeconds = 5;
    const localai::ChatResult result = Send(request);
    REQUIRE(HasCode(result, "ai.connect"));
    CHECK(result.diagnostics[0].message.find("ollama.com") != std::string::npos);
}

TEST_CASE("Cancel stops a waiting request at once", "[localai]")
{
    FakeOllama ollama;
    localai::CancelToken cancel;
    std::thread canceller([&] {
        std::this_thread::sleep_for(200ms);
        cancel.Cancel();
    });
    const auto start = std::chrono::steady_clock::now();
    const localai::ChatResult result = localai::Chat(ollama.Request("slow"), cancel);
    canceller.join();
    CHECK(HasCode(result, "ai.cancel"));
    CHECK(std::chrono::steady_clock::now() - start < 2s);
}

TEST_CASE("A chat job runs in the background, one at a time", "[localai]")
{
    FakeOllama ollama;
    {
        localai::ChatJob job;
        REQUIRE(job.Start(ollama.Request("good")));
        CHECK_FALSE(job.Start(ollama.Request("good"))); // still busy, or finished but not taken
        std::optional<localai::ChatResult> result;
        for (int i = 0; i < 500 && !result; ++i)
        {
            std::this_thread::sleep_for(10ms);
            result = job.Take();
        }
        REQUIRE(result);
        CHECK(result->ok);
        CHECK_FALSE(job.Take()); // only once

        // Cancel mid-wait, then start again: the old result never leaks into the new job.
        REQUIRE(job.Start(ollama.Request("slow")));
        std::this_thread::sleep_for(100ms);
        job.Cancel();
        for (int i = 0; i < 200 && job.Running(); ++i)
            std::this_thread::sleep_for(10ms);
        REQUIRE(job.Take()->diagnostics[0].code == "ai.cancel");
    }
    // Destroying a busy job cancels and joins quickly.
    const auto start = std::chrono::steady_clock::now();
    {
        localai::ChatJob job;
        REQUIRE(job.Start(ollama.Request("slow")));
        std::this_thread::sleep_for(50ms);
    }
    CHECK(std::chrono::steady_clock::now() - start < 2s);
}

TEST_CASE("Idea to validated plan through the bridge", "[localai][draft]")
{
    FakeOllama ollama;
    const std::filesystem::path assets = UMBRA_TEST_ASSET_DIRECTORY;
    std::ifstream in(assets / "rpg/reference.rpg.json");
    const authoring::Json rpg = authoring::Json::parse(in);

    localai::ChatRequest request = localai::MakeDraftRequest(rpg, "elder", "wolves near the well", "good", "http://127.0.0.1:" + std::to_string(ollama.port));
    REQUIRE(request.messages.size() == 2);
    CHECK(request.messages[0].content.find("sun_relic (Sun Relic)") != std::string::npos);
    CHECK(request.messages[0].content.find("Elder Mara") != std::string::npos);
    CHECK(request.format["required"].size() == 4);

    const localai::ChatResult result = Send(request);
    REQUIRE(result.ok);
    authoring::Json draft;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(localai::ParseDraft(result.text, draft, diagnostics));
    authoring::DraftPlan plan;
    CHECK(authoring::PlanDraft(rpg, draft, "elder", authoring::ScanScenes(assets / "scenes"), plan, diagnostics));

    // Asking for a fix sends the old answer back with the problems.
    localai::AddFixRequest(request, result.text, {{core::Severity::Error, "draft.next", "draft", "nodes[0].choices[0].next", "names no node"}});
    REQUIRE(request.messages.size() == 4);
    CHECK(request.messages[2].role == "assistant");
    CHECK(request.messages[3].content.find("nodes[0].choices[0].next: names no node") != std::string::npos);
    CHECK_FALSE(localai::ParseDraft("Sure! Here is your quest:", draft, diagnostics));
}
