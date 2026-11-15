#include "NightTestData.hpp"

#include <Words.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

using namespace umbra::night;
using namespace std::chrono_literals;

namespace
{
    // A stand-in model: answers with `answer`, after `gate` opens (if one is set).
    struct StandIn final : umbra::town::Mind
    {
        std::function<std::string(const umbra::town::MindRequest &)> answer;
        std::atomic<bool> *gate = nullptr;
        std::atomic<int> *asked = nullptr;
        std::string Name() const override { return "stand-in"; }
        std::string Complete(const umbra::town::MindRequest &request) override
        {
            if (asked)
                ++*asked;
            while (gate && !gate->load())
                std::this_thread::sleep_for(1ms);
            return answer(request);
        }
    };

    std::unique_ptr<umbra::town::Mind> Make(std::string line, std::atomic<bool> *gate = nullptr, std::atomic<int> *asked = nullptr)
    {
        auto mind = std::make_unique<StandIn>();
        mind->answer = [line](const umbra::town::MindRequest &) { return line; };
        mind->gate = gate;
        mind->asked = asked;
        return mind;
    }

    LineAsk Ask(const NightDef &def)
    {
        const int thanh = NightDef::Find(def.guests, "thanh"), lan = NightDef::Find(def.guests, "lan");
        return {LineKind::Caption, thanh, false, {thanh, lan}, "Le Thanh talks with Tran Ngoc Lan.", 1};
    }

    // Waits up to 2 s for the ticket.
    bool WaitReady(const Words &words, int ticket)
    {
        for (int i = 0; i < 2000 && !words.Ready(ticket); ++i)
            std::this_thread::sleep_for(1ms);
        return words.Ready(ticket);
    }

    const std::string worded = "Le Thanh leans in and whispers to Tran Ngoc Lan.";
}

TEST_CASE("Words: a quick answer is shown, anything else gives the template (T134)", "[night][words]")
{
    const NightDef def = Data();
    SECTION("a quick, good answer")
    {
        Words words(def, Make(worded));
        const int ticket = words.Ask(Ask(def));
        REQUIRE(WaitReady(words, ticket));
        CHECK(words.Take(ticket, "template") == worded);
    }
    SECTION("an empty or refused answer")
    {
        for (const char *answer : {"", "Le Thanh talks with Tran Ngoc Lan 3 times."})
        {
            Words words(def, Make(answer));
            const int ticket = words.Ask(Ask(def));
            REQUIRE(WaitReady(words, ticket));
            CHECK(words.Take(ticket, "template") == "template");
        }
    }
    SECTION("no model: every line is its template at once")
    {
        Words words(def, nullptr);
        const int ticket = words.Ask(Ask(def));
        CHECK(words.Ready(ticket));
        CHECK(words.Take(ticket, "template") == "template");
    }
}

TEST_CASE("Words: a late answer is dropped, not shown later (T134)", "[night][words]")
{
    const NightDef def = Data();
    std::atomic<bool> gate{false};
    Words words(def, Make(worded, &gate));
    const int late = words.Ask(Ask(def));
    std::this_thread::sleep_for(20ms);
    CHECK_FALSE(words.Ready(late));
    CHECK(words.Take(late, "template") == "template");
    gate = true;
    // The next line gets its own answer; the late one is gone.
    const int next = words.Ask(Ask(def));
    REQUIRE(WaitReady(words, next));
    CHECK(words.Take(next, "template") == worded);
    CHECK(words.Ready(late));
    CHECK(words.Take(late, "again") == "again");
}

TEST_CASE("Words: at most four lines wait, and Stop does not run them (T134)", "[night][words]")
{
    const NightDef def = Data();
    std::atomic<bool> gate{false};
    std::atomic<int> asked{0};
    Words words(def, Make(worded, &gate, &asked));
    const int running = words.Ask(Ask(def));
    for (int i = 0; i < 2000 && asked == 0; ++i)
        std::this_thread::sleep_for(1ms);
    std::vector<int> waiting;
    for (int i = 0; i < 5; ++i)
        waiting.push_back(words.Ask(Ask(def)));
    // 1. The oldest waiting line was dropped: ready at once, as its template.
    CHECK(words.Ready(waiting[0]));
    CHECK(words.Take(waiting[0], "template") == "template");
    for (std::size_t i = 1; i < waiting.size(); ++i)
        CHECK_FALSE(words.Ready(waiting[i]));
    CHECK_FALSE(words.Ready(running));
    // 2. Stop waits for the running answer only.
    std::thread open([&] {
        std::this_thread::sleep_for(50ms);
        gate = true;
    });
    const auto start = std::chrono::steady_clock::now();
    words.Stop();
    open.join();
    CHECK(std::chrono::steady_clock::now() - start < 1s);
    CHECK(asked == 1);
}
