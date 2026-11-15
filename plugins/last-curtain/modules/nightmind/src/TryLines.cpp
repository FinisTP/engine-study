#include "NightMind.hpp"

#include <NightBots.hpp>

#include <algorithm>
#include <chrono>

namespace umbra::night
{
    namespace
    {
        const char *KindName(LineKind kind)
        {
            switch (kind)
            {
            case LineKind::Caption: return "caption";
            case LineKind::Question: return "question";
            case LineKind::GoalDone: return "goal done";
            case LineKind::GoalFailed: return "goal failed";
            case LineKind::Turn: return "the Turn";
            case LineKind::Taken: return "Taken";
            }
            return "";
        }

        // Asks, guards and prints one line.
        void Try(const NightDef &def, umbra::town::Mind &mind, const LineAsk &ask, LineReport &report, std::ostream &out)
        {
            const auto start = std::chrono::steady_clock::now();
            const std::string answer = mind.Complete(Request(def, ask));
            const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            const bool accepted = !Guard(def, ask, answer).empty();
            out << "  [" << KindName(ask.kind) << "] " << (ask.kind == LineKind::Caption ? "narrator" : def.guests[static_cast<std::size_t>(ask.speaker)].name)
                << "\n    template: " << ask.line << "\n    model:    " << answer << "\n    " << (accepted ? "accepted" : "REFUSED") << ", " << ms << " ms\n";
            ++report.lines, report.accepted += accepted ? 1 : 0, report.totalMs += ms, report.slowestMs = std::max(report.slowestMs, ms);
        }
    }

    LineReport TryLines(const NightDef &def, const Captions &captions, umbra::town::Mind &mind, int nights, std::uint64_t seed, int perNight, std::ostream &out)
    {
        LineReport report;
        for (int n = 1; n <= nights; ++n)
        {
            out << "night " << n << "\n";
            NightState state = NewNight(def, seed + static_cast<std::uint64_t>(n - 1), -1);
            // Spread over the whole night: every 15th caption, every 3rd chat question, every Turn.
            int asked = 0, captionsSeen = 0, questionsSeen = 0;
            for (int step = 0; step < 4000 && !state.over; ++step)
            {
                // 1. A bot acts; the narrator tells it as the actor sees it.
                const int actor = Current(state);
                if (actor < 0)
                    break;
                const Action action = ChooseAction(def, state, actor);
                const NightState was = state;
                if (!Apply(def, state, action))
                    break;
                const std::vector<LogEntry> entries = NewLog(state, was.logCount);
                const Beat beat = Narrate(def, was, state, entries, action, actor, actor, captions);
                const auto seedOf = [&](int kind) { return static_cast<std::uint32_t>(state.seed + was.logCount * 4U + static_cast<std::uint32_t>(kind)); };
                // 2. Its caption, a chat question it opened, and the Turn.
                const bool taken = IsTaken(was, actor);
                if (!beat.caption.empty() && captionsSeen++ % 15 == 0 && asked < perNight)
                    ++asked, Try(def, mind, {taken ? LineKind::Taken : LineKind::Caption, actor, taken, Named(def, beat.caption), beat.caption, seedOf(0)}, report, out);
                if (state.chat.active && (!was.chat.active || was.chat.question != state.chat.question) && questionsSeen++ % 3 == 0 && asked < perNight)
                {
                    const std::string &text = def.questions[static_cast<std::size_t>(state.chat.question)].text;
                    ++asked, Try(def, mind, {LineKind::Question, state.chat.asker, false, Named(def, text), text, seedOf(1)}, report, out);
                }
                for (const LogEntry &e : entries)
                    if (e.kind == LogKind::Turned)
                    {
                        const GuestDef &g = def.guests[static_cast<std::size_t>(e.who)];
                        Try(def, mind, {LineKind::Turn, e.who, false, {e.who}, g.name + " becomes " + g.taken + ".", seedOf(2)}, report, out);
                    }
            }
        }
        out << "\n" << report.accepted << " of " << report.lines << " accepted; " << (report.lines ? report.totalMs / report.lines : 0) << " ms a line on average, slowest " << report.slowestMs << " ms\n";
        return report;
    }
}
