#include "TownMind.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace umbra::town
{
    namespace
    {
        // One word for how the speaker feels towards the player, so the voice fits.
        const char *Tone(const TownState &state, int resident)
        {
            const int regard = state.regard[static_cast<std::size_t>(resident)];
            return regard >= 60 ? "warm" : regard <= -60 ? "cold" : "plain";
        }
    }

    MindRequest WordingRequest(const TownDef &def, const TownState &state, const Fact &fact, int speaker)
    {
        const std::string &name = def.residents[static_cast<std::size_t>(speaker)].name;
        MindRequest request;
        // A small model follows an example far better than a description: one worked line, then ours.
        // (The example's people are not residents, so the guard would catch them being copied.)
        request.prompt = "Reword one line of dialogue for a villager in a game. Keep every name and the meaning, add nothing. "
                         "Reply with the new line only: one short spoken sentence.\n\n"
                         "Speaker: Marta (tone: plain)\nLine: They say Jonas has fallen out with Petra.\nReworded: Word is Jonas and Petra are not speaking any more.\n\n"
                         "Speaker: " + name + " (tone: " + Tone(state, speaker) + ")\nLine: " + Says(def, fact, speaker) + "\nReworded:";
        // One plain sentence: a capital letter, then letters, spaces, commas and apostrophes, ending properly.
        request.grammar = "root ::= \" \"? [A-Z] [a-zA-Z ,'-]{8,110} [.!?]\n";
        request.maxTokens = 40;
        request.seed = static_cast<std::uint32_t>(state.seed ^ static_cast<std::uint64_t>(fact.tick * 31 + speaker));
        return request;
    }

    MindRequest ChoiceRequest(const TownDef &def, const TownState &state, int resident, const std::vector<Intent> &legal)
    {
        const std::string &name = def.residents[static_cast<std::size_t>(resident)].name;
        MindRequest request;
        request.prompt = "You are " + name + ", who lives in a small town. It is " + std::to_string(state.Hour()) + ":00. You have " +
                         std::to_string(state.residents[static_cast<std::size_t>(resident)].money) + " coins.\n";
        for (std::size_t n = 0; n < def.needs.size(); ++n)
            request.prompt += def.needs[n].name + ": " + std::to_string(state.Need(static_cast<std::size_t>(resident), n) / 10) + " of 100\n";
        request.prompt += "What do you do next? Answer with the number only.\n";
        request.grammar = "root ::= ";
        for (std::size_t i = 0; i < legal.size(); ++i)
        {
            const ActivityDef &a = def.activities[static_cast<std::size_t>(legal[i].activity)];
            request.prompt += std::to_string(i + 1) + ". " + a.name + (a.cost > 0 ? " (costs " + std::to_string(a.cost * a.hours) + ")" : "") + "\n";
            request.grammar += std::string(i ? " | " : "") + "\"" + std::to_string(i + 1) + "\"";
        }
        request.prompt += "Answer: ";
        request.grammar += "\n";
        request.maxTokens = 3;
        request.seed = static_cast<std::uint32_t>(state.seed ^ static_cast<std::uint64_t>(state.tick * 131 + resident));
        return request;
    }

    TownMind::TownMind(std::unique_ptr<Mind> mind, LineGuard guard, bool threaded) : mind_(std::move(mind)), guard_(std::move(guard)), threaded_(threaded)
    {
        if (threaded_)
            worker_ = std::thread([this] { Work(); });
    }

    TownMind::~TownMind()
    {
        {
            const std::lock_guard lock(mutex_);
            stop_ = true;
        }
        wake_.notify_all();
        if (worker_.joinable())
            worker_.join(); // waits for the answer being written, if any (Mind.md "Shutting down")
    }

    void TownMind::Work()
    {
        for (;;)
        {
            Job job;
            {
                std::unique_lock lock(mutex_);
                wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
                if (stop_)
                    return;
                job = std::move(queue_.front());
                queue_.pop_front();
            }
            job.answer = mind_->Complete(job.request);
            const std::lock_guard lock(mutex_);
            done_.push_back(std::move(job));
        }
    }

    void TownMind::Send(Job job)
    {
        job.generation = generation_;
        ++stats_.asked;
        if (!threaded_)
        {
            job.answer = mind_->Complete(job.request);
            Take(job);
            return;
        }
        {
            const std::lock_guard lock(mutex_);
            if (queue_.size() >= maxQueued)
                queue_.pop_front(); // the oldest question is the least useful by now
            queue_.push_back(std::move(job));
        }
        wake_.notify_one();
    }

    void TownMind::Collect()
    {
        std::deque<Job> done;
        {
            const std::lock_guard lock(mutex_);
            done.swap(done_);
        }
        for (Job &job : done)
            Take(job);
    }

    void TownMind::Forget()
    {
        ++generation_; // answers still on their way are dropped when they arrive
        const std::lock_guard lock(mutex_);
        queue_.clear();
        lines_.clear();
        picks_.clear();
    }

    void TownMind::Take(Job &job)
    {
        if (job.generation != generation_ || static_cast<std::size_t>(job.resident) >= lines_.size())
            return;
        if (job.choice)
        {
            // "2" -> the second option that was legal when asked. Checked again when it is used.
            const int number = job.answer.empty() ? 0 : std::atoi(job.answer.c_str());
            Pick &pick = picks_[static_cast<std::size_t>(job.resident)];
            if (number >= 1 && static_cast<std::size_t>(number) <= job.options.size())
                pick.intent = job.options[static_cast<std::size_t>(number - 1)];
            return;
        }
        Ready &ready = lines_[static_cast<std::size_t>(job.resident)];
        if (!ready.asked || !SameFact(ready.fact, job.fact))
            return; // they have other news by now
        const std::string line = Tidy(job.answer);
        const std::string objection = line.empty() ? "no answer" : guard_.Objection({*def_, job.fact, job.resident}, line);
        if (!objection.empty())
        {
            ++stats_.refused;
            stats_.lastObjection = objection + ": " + line;
            return; // `has` stays false: the template will be used
        }
        ready.line = line;
        ready.has = true;
    }

    void TownMind::Prepare(const TownDef &def, const TownState &state, int resident)
    {
        def_ = &def;
        lines_.resize(state.residents.size());
        picks_.resize(state.residents.size());
        const int news = NextNews(def, state, resident);
        if (news < 0)
            return;
        const Fact &fact = state.facts[static_cast<std::size_t>(news)];
        Ready &ready = lines_[static_cast<std::size_t>(resident)];
        if (ready.asked && SameFact(ready.fact, fact))
            return; // already asked (or answered) for exactly this
        ready = {true, false, fact, {}};
        Job job;
        job.resident = resident;
        job.fact = fact;
        job.request = WordingRequest(def, state, fact, resident);
        Send(std::move(job));
    }

    std::string TownMind::Talk(const TownDef &def, TownState &state, int resident)
    {
        Collect();
        const int news = NextNews(def, state, resident);
        std::string worded;
        if (news >= 0 && static_cast<std::size_t>(resident) < lines_.size())
        {
            Ready &ready = lines_[static_cast<std::size_t>(resident)];
            if (ready.has && SameFact(ready.fact, state.facts[static_cast<std::size_t>(news)]))
                worded = ready.line, ++stats_.worded;
            else
                ++stats_.late;
            ready = {};
        }
        else if (news >= 0)
            ++stats_.late;
        return TalkTo(def, state, resident, worded);
    }

    void TownMind::Think(const TownDef &def, const TownState &state)
    {
        def_ = &def;
        lines_.resize(state.residents.size());
        picks_.resize(state.residents.size());
        for (int i = 0; i < static_cast<int>(state.residents.size()); ++i)
        {
            const ResidentState &r = state.residents[static_cast<std::size_t>(i)];
            Pick &pick = picks_[static_cast<std::size_t>(i)];
            // Free within three ticks, and not asked for that moment yet.
            if (r.until - state.tick > 3 || r.until <= state.tick || pick.forTick == r.until)
                continue;
            // What will be allowed can only be judged as if they were free now.
            TownState freed = state;
            freed.residents[static_cast<std::size_t>(i)].until = freed.tick;
            Job job;
            job.choice = true;
            job.resident = i;
            job.options = LegalIntents(def, freed, i);
            if (job.options.size() < 2)
                continue;
            pick = {true, r.until, std::nullopt};
            job.request = ChoiceRequest(def, freed, i, job.options);
            Send(std::move(job));
        }
    }

    std::optional<Intent> TownMind::Decide(const TownDef &def, const TownState &state, int resident, void *user)
    {
        TownMind &mind = *static_cast<TownMind *>(user);
        if (static_cast<std::size_t>(resident) >= mind.picks_.size())
            return std::nullopt;
        Pick &pick = mind.picks_[static_cast<std::size_t>(resident)];
        const std::optional<Intent> intent = std::exchange(pick.intent, std::nullopt);
        if (!intent)
            return std::nullopt;
        // Emergencies stay with the rules: a resident with an urgent need is not the model's to steer.
        for (std::size_t n = 0; n < def.needs.size(); ++n)
            if (state.Need(static_cast<std::size_t>(resident), n) >= def.urgent)
                return std::nullopt;
        // Tick checks again; counted here so the numbers show how often the model is usable.
        ++(Legal(def, state, *intent) == Refusal::None ? mind.stats_.chosen : mind.stats_.illegal);
        return intent;
    }
}
