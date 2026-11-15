#include "Words.hpp"

#include <algorithm>

namespace umbra::night
{
    namespace
    {
        constexpr std::size_t maxWaiting = 4;
    }

    Words::Words(const NightDef &def, std::unique_ptr<umbra::town::Mind> mind) : def_(def), mind_(std::move(mind))
    {
        if (mind_)
            worker_ = std::thread([this] { Work(); });
    }

    Words::~Words() { Stop(); }

    int Words::Ask(LineAsk ask)
    {
        std::lock_guard lock(mutex_);
        const int ticket = next_++;
        // 1. No model (or stopped): the template, at once.
        if (!mind_ || stop_)
        {
            done_[ticket] = {};
            return ticket;
        }
        // 2. Too many waiting: drop the oldest, so a slow model never builds a backlog.
        if (waiting_.size() >= maxWaiting)
        {
            done_[waiting_.front().first] = {};
            waiting_.pop_front();
        }
        waiting_.emplace_back(ticket, std::move(ask));
        wake_.notify_one();
        return ticket;
    }

    bool Words::Ready(int ticket) const
    {
        std::lock_guard lock(mutex_);
        if (done_.count(ticket) != 0)
            return true;
        const bool waiting = std::any_of(waiting_.begin(), waiting_.end(), [&](const auto &job) { return job.first == ticket; });
        return !waiting && ticket != running_;
    }

    std::string Words::Take(int ticket, const std::string &fallback)
    {
        std::lock_guard lock(mutex_);
        // 1. Still waiting or running: forget it, the template shows.
        std::erase_if(waiting_, [&](const auto &job) { return job.first == ticket; });
        if (ticket == running_)
            forgotten_.insert(ticket);
        // 2. In: the answer, unless the guard refused it.
        const auto found = done_.find(ticket);
        if (found == done_.end())
            return fallback;
        std::string line = std::move(found->second);
        done_.erase(found);
        return line.empty() ? fallback : line;
    }

    void Words::Stop()
    {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
            waiting_.clear();
        }
        wake_.notify_all();
        if (worker_.joinable())
            worker_.join();
    }

    void Words::Work()
    {
        // 1. The first answer is the slow one on a graphics card: pay it now.
        mind_->WarmUp();
        while (true)
        {
            std::pair<int, LineAsk> job;
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock, [&] { return stop_ || !waiting_.empty(); });
                if (stop_)
                    return;
                job = std::move(waiting_.front());
                waiting_.pop_front();
                running_ = job.first;
            }
            // 2. Ask the model without the lock, then guard its answer.
            std::string line = Guard(def_, job.second, mind_->Complete(Request(def_, job.second)));
            std::lock_guard lock(mutex_);
            running_ = 0;
            if (forgotten_.erase(job.first) == 0)
                done_[job.first] = std::move(line);
        }
    }
}
