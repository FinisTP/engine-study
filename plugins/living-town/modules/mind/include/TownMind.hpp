#pragma once

#include "LineGuard.hpp"
#include "Mind.hpp"

#include <Town.hpp>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

// The town's use of a model (S23, Mind.md): it words the news residents tell and picks among the
// activities the rules allow. It only ever proposes: the rules check every choice (`Legal`) and
// the guard every line, and whatever is late or refused falls back to the rules and templates.
namespace umbra::town
{
    // The request texts (also used by tests and the tool to show what the model is asked).
    [[nodiscard]] MindRequest WordingRequest(const TownDef &def, const TownState &state, const Fact &fact, int speaker);
    [[nodiscard]] MindRequest ChoiceRequest(const TownDef &def, const TownState &state, int resident, const std::vector<Intent> &legal);

    struct MindStats final
    {
        int asked = 0;     // requests sent to the model
        int worded = 0;    // lines shown in the model's words
        int refused = 0;   // lines the guard turned down (the template was shown)
        int late = 0;      // talks where no line was ready (the template was shown)
        int chosen = 0;    // activities the model picked that the rules accepted
        int illegal = 0;   // picks that were not (or no longer) legal: the rules chose
        std::string lastObjection;
    };

    class TownMind final
    {
    public:
        // `threaded` false: requests are answered at once on the caller (tests, tools).
        TownMind(std::unique_ptr<Mind> mind, LineGuard guard, bool threaded = true);
        ~TownMind();
        TownMind(const TownMind &) = delete;
        TownMind &operator=(const TownMind &) = delete;

        // Has the model word what `resident` would tell next, ahead of the player asking.
        void Prepare(const TownDef &def, const TownState &state, int resident);
        // TalkTo, in the model's words when a checked line is ready; else the template. Never waits.
        std::string Talk(const TownDef &def, TownState &state, int resident);

        // Asks the model what residents who become free within the half hour should do next.
        void Think(const TownDef &def, const TownState &state);
        // Use as Tick's `decide` with `user` = this: the model's pick for a free resident, if one came.
        static std::optional<Intent> Decide(const TownDef &def, const TownState &state, int resident, void *user);

        // Takes finished answers from the worker (call once per frame). Forget() drops everything
        // asked so far: after loading a game or starting a new one.
        void Collect();
        void Forget();
        [[nodiscard]] const MindStats &Stats() const { return stats_; }
        [[nodiscard]] std::string ModelName() const { return mind_->Name(); }

    private:
        struct Job final
        {
            int generation = 0;
            bool choice = false;
            int resident = 0;
            Fact fact;               // wording: the fact asked about
            std::vector<Intent> options; // choice: what was legal when asked
            MindRequest request;
            std::string answer;
        };
        void Send(Job job);
        void Work();
        void Take(Job &job);
        static bool SameFact(const Fact &a, const Fact &b) { return a.tick == b.tick && a.kind == b.kind && a.who == b.who && a.other == b.other && a.what == b.what; }

        std::unique_ptr<Mind> mind_;
        LineGuard guard_;
        MindStats stats_;
        int generation_ = 0;
        const TownDef *def_ = nullptr; // for the guard, set by Prepare
        // Per resident: the line ready to say (for which fact), and the activity picked.
        struct Ready final
        {
            bool asked = false, has = false;
            Fact fact;
            std::string line;
        };
        std::vector<Ready> lines_;
        struct Pick final
        {
            bool asked = false;
            std::int32_t forTick = -1; // the tick the resident becomes free
            std::optional<Intent> intent;
        };
        std::vector<Pick> picks_;

        // The worker: one thread, a short queue; answers come back through `done_`.
        bool threaded_ = true, stop_ = false;
        std::thread worker_;
        std::mutex mutex_;
        std::condition_variable wake_;
        std::deque<Job> queue_, done_;
        static constexpr std::size_t maxQueued = 6;
    };
}
