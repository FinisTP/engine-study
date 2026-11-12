#include "Soak.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <sstream>

namespace umbra::town
{
    namespace
    {
        bool Any(const std::vector<Pattern> &patterns, const Trouble &trouble)
        {
            return std::any_of(patterns.begin(), patterns.end(), [&](const Pattern &p) { return p.Matches(trouble); });
        }

        // The soak test's own reading of the plan, apart from the storyteller's filter (so a bug
        // in the filter shows here): was this trouble against the plan?
        bool Broke(const PlanDef &plan, int waiting, int day, const Trouble &trouble)
        {
            const BeatDef *beat = waiting >= 0 ? &plan.beats[static_cast<std::size_t>(waiting)] : nullptr;
            if (Any(plan.never, trouble) || (beat != nullptr && Any(beat->forbid, trouble)))
                return true;
            if (trouble.kind != TroubleKind::Leave && trouble.kind != TroubleKind::Death)
                return false;
            const bool forBeat = beat != nullptr && Any(beat->outcomes, trouble) && day >= beat->from && day <= beat->to;
            return !forBeat && std::none_of(plan.allow.begin(), plan.allow.end(), [&](const Pattern &p) { return p.Matches(trouble) && day >= p.from && day <= p.to; });
        }
    }

    SoakReport Soak(const TownDef &def, const PlanDef &plan, std::uint64_t firstSeed, int seeds, int days)
    {
        SoakReport report;
        report.seeds = seeds, report.days = days;
        report.beats.resize(plan.beats.size());
        for (int s = 0; s < seeds; ++s)
        {
            // 1. One playthrough with nobody playing; every trouble is checked as it happens.
            TownState state = NewTown(def, firstSeed + static_cast<std::uint64_t>(s));
            StoryState story = NewStory(plan);
            for (int tick = 0; tick < days * ticksPerDay; ++tick)
            {
                Tick(def, state);
                const int troubles = story.troubles, waiting = story.Next();
                StoryStep(def, plan, state, story);
                if (story.troubles == troubles)
                    continue;
                ++report.troubles;
                const auto last = std::find_if(state.events.rbegin(), state.events.rend(), [](const TownEvent &e) { return e.kind == EventKind::Trouble; });
                report.forbidden += Broke(plan, waiting, state.Day(), {static_cast<TroubleKind>(last->what), last->who, last->other});
            }
            int gone = 0;
            for (const ResidentState &r : state.residents)
                gone += r.gone != Gone::No;
            report.forbidden += gone > plan.maxGone;
            // 2. What became of each beat, and the road as a whole.
            std::string road;
            bool complete = true;
            for (std::size_t b = 0; b < plan.beats.size(); ++b)
            {
                const BeatState &beat = story.beats[b];
                SoakBeat &tally = report.beats[b];
                const bool reached = beat.how == BeatHow::Natural || beat.how == BeatHow::Forced;
                const std::string outcome = reached ? Sentence(def, beat.outcome) : "(not reached)";
                tally.natural += beat.how == BeatHow::Natural, tally.forced += beat.how == BeatHow::Forced, tally.missed += !reached;
                if (reached)
                {
                    const int day = beat.tick / ticksPerDay + 1;
                    tally.firstDay = tally.firstDay == 0 ? day : std::min(tally.firstDay, day);
                    tally.lastDay = std::max(tally.lastDay, day);
                    ++tally.outcomes[outcome];
                }
                complete = complete && reached;
                road += (b ? " / " : "") + outcome;
            }
            report.complete += complete;
            ++report.roads[road];
        }
        return report;
    }

    std::string Text(const PlanDef &plan, const SoakReport &report)
    {
        std::ostringstream out;
        const auto percent = [&](int count) { return std::to_string(report.seeds ? (count * 100 + report.seeds / 2) / report.seeds : 0) + "%"; };
        out << report.seeds << " playthroughs of " << report.days << " days\n"
            << "plot finished: " << report.complete << " (" << percent(report.complete) << ")   against the plan: " << report.forbidden
            << "   troubles per playthrough: " << (report.seeds ? static_cast<double>(report.troubles) / report.seeds : 0.0) << "\n";
        for (std::size_t b = 0; b < plan.beats.size(); ++b)
        {
            const SoakBeat &beat = report.beats[b];
            out << "\nbeat \"" << plan.beats[b].id << "\" (days " << plan.beats[b].from << "-" << plan.beats[b].to << "): by itself " << percent(beat.natural)
                << ", forced " << percent(beat.forced) << ", missed " << beat.missed << "; reached on days " << beat.firstDay << "-" << beat.lastDay << "; "
                << beat.outcomes.size() << " different outcomes\n";
            // The most common outcomes first (at most eight).
            std::vector<std::pair<int, std::string>> sorted;
            for (const auto &[outcome, count] : beat.outcomes)
                sorted.emplace_back(-count, outcome);
            std::sort(sorted.begin(), sorted.end());
            for (std::size_t i = 0; i < sorted.size() && i < 8; ++i)
                out << "  " << percent(-sorted[i].first) << "  " << sorted[i].second << "\n";
            if (sorted.size() > 8)
                out << "  ... and " << sorted.size() - 8 << " more\n";
        }
        int commonest = 0;
        for (const auto &[road, count] : report.roads)
            commonest = std::max(commonest, count);
        out << "\ndifferent roads through the plot: " << report.roads.size() << " (the most common one: " << percent(commonest) << " of playthroughs)\n";
        return out.str();
    }

    nlohmann::json Json(const PlanDef &plan, const SoakReport &report)
    {
        nlohmann::json out = {{"seeds", report.seeds}, {"days", report.days}, {"complete", report.complete}, {"forbidden", report.forbidden},
                              {"troubles", report.troubles}, {"roads", report.roads.size()}, {"holds", report.Holds()}};
        for (std::size_t b = 0; b < plan.beats.size(); ++b)
            out["beats"].push_back({{"id", plan.beats[b].id}, {"natural", report.beats[b].natural}, {"forced", report.beats[b].forced}, {"missed", report.beats[b].missed},
                                    {"firstDay", report.beats[b].firstDay}, {"lastDay", report.beats[b].lastDay}, {"outcomes", report.beats[b].outcomes}});
        return out;
    }
}
