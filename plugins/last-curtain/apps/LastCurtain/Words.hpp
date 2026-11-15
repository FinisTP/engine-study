#pragma once

#include <NightMind.hpp>

#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

// The model's lines on a worker thread (plan 3, NightMind.md): Ask when a line is made, show it when
// Ready (or when the wait is over) with Take. No model, a late, empty or refused answer: the template.
namespace umbra::night
{
    class Words
    {
    public:
        Words(const NightDef &def, std::unique_ptr<umbra::town::Mind> mind); // null = no model
        ~Words();
        Words(const Words &) = delete;
        Words &operator=(const Words &) = delete;

        [[nodiscard]] bool HasModel() const { return mind_ != nullptr; }
        int Ask(LineAsk ask);                // a ticket; the oldest waiting line is dropped past 4
        [[nodiscard]] bool Ready(int ticket) const; // answered, refused, dropped or unknown
        // The guarded answer if it is in, else the fallback; the ticket is forgotten either way.
        std::string Take(int ticket, const std::string &fallback);
        void Stop(); // drops waiting lines, waits for the running one only

    private:
        void Work();

        const NightDef &def_;
        std::unique_ptr<umbra::town::Mind> mind_;
        mutable std::mutex mutex_;
        std::condition_variable wake_;
        std::deque<std::pair<int, LineAsk>> waiting_;
        std::map<int, std::string> done_; // ticket -> guarded answer ("" = use the template)
        std::set<int> forgotten_;         // taken while running: drop its answer
        int running_ = 0;
        int next_ = 1;
        bool stop_ = false;
        std::thread worker_;
    };
}
