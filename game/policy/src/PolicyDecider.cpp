#include <PolicyDecider.hpp>

// A browser build without threads (P10) decides at once on the caller's thread: the network takes
// microseconds, and the caller still collects the answer with Take, so nothing else changes.
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
#define UMBRA_POLICY_INLINE 1
#endif

namespace umbra::policy
{
    PolicyDecider::PolicyDecider(Network network, std::chrono::milliseconds delay) : network_(std::move(network)), delay_(delay)
    {
#ifndef UMBRA_POLICY_INLINE
        worker_ = std::thread([this] { Run(); });
#endif
    }

    PolicyDecider::~PolicyDecider()
    {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        wake_.notify_all();
        if (worker_.joinable())
            worker_.join();
    }

    bool PolicyDecider::Request(std::uint64_t id, std::vector<float> observation, std::vector<std::uint8_t> mask)
    {
#ifdef UMBRA_POLICY_INLINE
        answer_ = std::make_pair(id, network_.Choose(observation, mask));
        return true;
#endif
        {
            std::lock_guard lock(mutex_);
            if (busy_)
                return false;
            busy_ = true;
            jobId_ = id;
            observation_ = std::move(observation);
            mask_ = std::move(mask);
            answer_.reset();
        }
        wake_.notify_all();
        return true;
    }

    std::optional<int> PolicyDecider::Take(std::uint64_t id)
    {
        std::lock_guard lock(mutex_);
        if (!answer_)
            return std::nullopt;
        const auto [answerId, action] = *answer_;
        answer_.reset(); // taken, or stale: either way it's gone
        if (answerId != id)
            return std::nullopt;
        return action;
    }

    void PolicyDecider::Run()
    {
        std::unique_lock lock(mutex_);
        while (true)
        {
            // 1. Sleep until there is a job or we are stopping.
            wake_.wait(lock, [&] { return stop_ || (busy_ && !answer_); });
            if (stop_)
                return;
            // 2. Work on copies without holding the lock.
            const std::uint64_t id = jobId_;
            const std::vector<float> observation = observation_;
            const std::vector<std::uint8_t> mask = mask_;
            lock.unlock();
            if (delay_.count() > 0)
                std::this_thread::sleep_for(delay_);
            const int action = network_.Choose(observation, mask);
            lock.lock();
            // 3. Publish the answer tagged with its request.
            answer_ = std::make_pair(id, action);
            busy_ = false;
        }
    }
}
