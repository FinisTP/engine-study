#include "Storyteller.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <climits>

namespace umbra::town
{
    namespace
    {
        bool Grave(TroubleKind kind) { return kind == TroubleKind::Leave || kind == TroubleKind::Death; }
        bool Any(const std::vector<Pattern> &patterns, const Trouble &trouble)
        {
            return std::any_of(patterns.begin(), patterns.end(), [&](const Pattern &p) { return p.Matches(trouble); });
        }

        // The storyteller's own dice: from the town's seed and clock, so it needs no state of its
        // own and does not disturb the town's dice.
        std::uint64_t Dice(const TownState &state, std::uint64_t salt)
        {
            std::uint64_t x = state.seed * 0x9E3779B97F4A7C15ULL + static_cast<std::uint64_t>(state.tick) * 0xBF58476D1CE4E5B9ULL + salt * 0x94D049BB133111EBULL + 1;
            x ^= x >> 31, x *= 0xD6E8FEB86659FD93ULL, x ^= x >> 29;
            return x;
        }

        // Rehearsal: lives a copy of the town `plan.lookAhead` days on (rules only) and scores it:
        // the sooner the town itself proposes something that reaches `beat`, the better; people in
        // a bad way count against. Storyteller.md "Looking ahead".
        int Rehearse(const TownDef &def, const PlanDef &plan, TownState fork, const StoryState &story, const BeatDef &beat)
        {
            int score = 0;
            for (int day = 1; day <= plan.lookAhead && score == 0; ++day)
            {
                Run(def, fork, ticksPerDay);
                for (const Trouble &proposal : Proposals(def, fork))
                    if (Any(beat.outcomes, proposal) && Permitted(def, plan, fork, story, proposal))
                        score = (plan.lookAhead - day + 1) * 100;
            }
            for (const ResidentState &r : fork.residents)
                score -= r.gone == Gone::No && r.crisis != 0 ? 10 : 0;
            return score;
        }
    }

    int StoryState::Next() const
    {
        for (std::size_t i = 0; i < beats.size(); ++i)
            if (beats[i].how == BeatHow::Waiting)
                return static_cast<int>(i);
        return -1;
    }

    StoryState NewStory(const PlanDef &plan)
    {
        StoryState story;
        story.beats.resize(plan.beats.size());
        return story;
    }

    bool Permitted(const TownDef &def, const PlanDef &plan, const TownState &state, const StoryState &story, const Trouble &trouble)
    {
        const int next = story.Next(), day = state.Day();
        const BeatDef *beat = next >= 0 ? &plan.beats[static_cast<std::size_t>(next)] : nullptr;
        // 1. It must be possible, and neither the plan nor the waiting beat may forbid it.
        if (!Possible(def, state, trouble) || Any(plan.never, trouble) || (beat != nullptr && Any(beat->forbid, trouble)))
            return false;
        // 2. What reaches the waiting beat must wait for its window.
        const bool reaches = beat != nullptr && Any(beat->outcomes, trouble);
        if (reaches && day < beat->from)
            return false;
        // 3. Pacing: not too many ill at once, not too many gone in all.
        int ill = 0, gone = 0;
        for (const ResidentState &r : state.residents)
            ill += r.gone == Gone::No && state.tick < r.illUntil, gone += r.gone != Gone::No;
        if (trouble.kind == TroubleKind::Ill && ill >= plan.maxIll)
            return false;
        if (!Grave(trouble.kind))
            return true;
        // 4. Nobody leaves or dies unless the plan says so: for the waiting beat, or in `allow`.
        const bool allowed = reaches || std::any_of(plan.allow.begin(), plan.allow.end(), [&](const Pattern &p) { return p.Matches(trouble) && day >= p.from && day <= p.to; });
        return allowed && gone < plan.maxGone;
    }

    void StoryStep(const TownDef &def, const PlanDef &plan, TownState &state, StoryState &story)
    {
        if (state.tick % ticksPerDay != plan.hour * ticksPerHour)
            return;
        const int day = state.Day();
        const auto warn = [&](const BeatDef &beat, const std::string &what) {
            if (story.warnings.size() == 32)
                story.warnings.erase(story.warnings.begin());
            story.warnings.push_back("day " + std::to_string(day) + ": beat \"" + beat.id + "\" " + what);
        };
        // 1. A beat whose window closed without it: missed (the soak test counts these).
        int next = story.Next();
        for (; next >= 0 && day > plan.beats[static_cast<std::size_t>(next)].to; next = story.Next())
        {
            story.beats[static_cast<std::size_t>(next)] = {BeatHow::Missed, state.tick, {}};
            warn(plan.beats[static_cast<std::size_t>(next)], "was missed: its window closed");
        }
        const BeatDef *beat = next >= 0 ? &plan.beats[static_cast<std::size_t>(next)] : nullptr;
        const auto act = [&](const Trouble &trouble, BeatHow how) {
            Happen(def, state, trouble);
            story.lastTroubleDay = day, story.lastKind = trouble.kind, ++story.troubles;
            if (how != BeatHow::Waiting)
                story.beats[static_cast<std::size_t>(next)] = {how, state.tick, trouble};
        };
        // 2. The filter: of what the town proposes, what the plan permits today.
        std::vector<Trouble> candidates;
        for (const Trouble &proposal : Proposals(def, state))
            if (Permitted(def, plan, state, story, proposal))
                candidates.push_back(proposal);

        // 3. The waiting beat's window is open: the plot goes first.
        if (beat != nullptr && day >= beat->from)
        {
            // 3a. The town got there by itself: one of its own proposals reaches the beat.
            std::vector<Trouble> natural;
            for (const Trouble &candidate : candidates)
                if (Any(beat->outcomes, candidate))
                    natural.push_back(candidate);
            if (!natural.empty())
                return act(natural[Dice(state, 1) % natural.size()], BeatHow::Natural);
            // 3b. Last day of the window: force it. Of everything that could be named, the
            //     outcome the town has led up to most (Likely).
            if (day == beat->to)
            {
                Trouble best;
                int bestScore = INT_MIN;
                const int count = static_cast<int>(state.residents.size());
                for (int kind = 0; kind < 5; ++kind)
                    for (int who = 0; who < count; ++who)
                        for (int other = -1; other < count; ++other)
                        {
                            const Trouble trouble{static_cast<TroubleKind>(kind), who, other};
                            if (!Any(beat->outcomes, trouble) || !Permitted(def, plan, state, story, trouble))
                                continue;
                            if (const int score = Likely(def, state, trouble); score > bestScore)
                                best = trouble, bestScore = score;
                        }
                if (bestScore == INT_MIN)
                {
                    story.beats[static_cast<std::size_t>(next)] = {BeatHow::Missed, state.tick, {}};
                    return warn(*beat, "was missed: none of its outcomes is possible any more");
                }
                act(best, BeatHow::Forced);
                return warn(*beat, "had to be forced: " + Sentence(def, best));
            }
        }
        // 4. Pacing: days of peace after trouble.
        if (candidates.empty() || day - story.lastTroubleDay <= plan.quietDays)
            return;
        // 5. The nudge: rehearse each candidate (and doing nothing) some days ahead; take the one
        //    after which the town comes to the beat soonest, if it beats doing nothing.
        if (beat != nullptr && plan.lookAhead > 0)
        {
            const int nothing = Rehearse(def, plan, state, story, *beat);
            int best = -1, bestScore = nothing;
            const std::size_t tries = std::min<std::size_t>(candidates.size(), 8), first = Dice(state, 2) % candidates.size();
            for (std::size_t i = 0; i < tries; ++i)
            {
                const std::size_t index = (first + i) % candidates.size();
                TownState fork = state;
                Happen(def, fork, candidates[index]);
                // Variety: the same kind of trouble twice running counts a little against.
                const int score = Rehearse(def, plan, fork, story, *beat) - (candidates[index].kind == story.lastKind && story.troubles > 0 ? 30 : 0);
                if (score > bestScore)
                    best = static_cast<int>(index), bestScore = score;
            }
            if (best >= 0)
                return act(candidates[static_cast<std::size_t>(best)], BeatHow::Waiting);
        }
        // 6. Otherwise the town has a life of its own: one day in three, something happens anyway.
        if (Dice(state, 3) % 3 == 0)
            act(candidates[Dice(state, 4) % candidates.size()], BeatHow::Waiting);
    }

    void RunStory(const TownDef &def, const PlanDef &plan, TownState &state, StoryState &story, int ticks)
    {
        for (int i = 0; i < ticks; ++i)
        {
            Tick(def, state);
            StoryStep(def, plan, state, story);
        }
    }

    nlohmann::json SaveStory(const StoryState &story)
    {
        nlohmann::json out = {{"format", "umbra.town.story"}, {"lastDay", story.lastTroubleDay}, {"troubles", story.troubles}, {"lastKind", static_cast<int>(story.lastKind)}};
        out["beats"] = nlohmann::json::array();
        for (const BeatState &b : story.beats)
            out["beats"].push_back({static_cast<int>(b.how), b.tick, static_cast<int>(b.outcome.kind), b.outcome.who, b.outcome.other});
        return out;
    }

    bool LoadStory(const PlanDef &plan, const nlohmann::json &saved, StoryState &out)
    {
        try
        {
            if (!saved.is_object() || saved.value("format", "") != "umbra.town.story" || saved.at("beats").size() != plan.beats.size())
                return false; // another plan's story
            StoryState story;
            story.lastTroubleDay = saved.at("lastDay").get<std::int32_t>();
            story.troubles = saved.at("troubles").get<std::int32_t>();
            story.lastKind = static_cast<TroubleKind>(std::clamp(saved.at("lastKind").get<int>(), 0, 4));
            for (const nlohmann::json &b : saved["beats"])
                story.beats.push_back({static_cast<BeatHow>(std::clamp(b.at(0).get<int>(), 0, 3)), b.at(1).get<std::int32_t>(),
                                       {static_cast<TroubleKind>(std::clamp(b.at(2).get<int>(), 0, 4)), b.at(3).get<int>(), b.at(4).get<int>()}});
            out = std::move(story);
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }
}
