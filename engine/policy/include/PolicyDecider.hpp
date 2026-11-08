#pragma once

#include <PolicyModel.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace umbra::policy
{
    // Runs the network away from the frame loop (roadmap P8A.5). One worker thread, at most one
    // job at a time; every answer carries the request id it was made for, so a late answer for an
    // old decision is simply never taken. The caller owns the deadline and the fallback.
    class PolicyDecider final
    {
    public:
        // `delay` is only for tests (a slow model).
        explicit PolicyDecider(Network network, std::chrono::milliseconds delay = std::chrono::milliseconds{0});
        ~PolicyDecider(); // stops and joins the worker; an unfinished job is dropped

        PolicyDecider(const PolicyDecider &) = delete;
        PolicyDecider &operator=(const PolicyDecider &) = delete;

        // Starts a decision on a copy of the snapshot. False if a job is still running (overload).
        bool Request(std::uint64_t id, std::vector<float> observation, std::vector<std::uint8_t> mask);
        // The answer for `id` once it is ready: an action slot, or -1 (no legal action / broken output).
        // Answers for any other id are dropped.
        std::optional<int> Take(std::uint64_t id);

    private:
        void Run();

        Network network_;
        std::chrono::milliseconds delay_;
        std::mutex mutex_;
        std::condition_variable wake_;
        bool stop_ = false;
        bool busy_ = false;
        std::uint64_t jobId_ = 0;
        std::vector<float> observation_;
        std::vector<std::uint8_t> mask_;
        std::optional<std::pair<std::uint64_t, int>> answer_;
        std::thread worker_; // last: started in the constructor body after everything above exists
    };
}
