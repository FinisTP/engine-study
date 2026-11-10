#include "Animator.hpp"

#include <algorithm>
#include <cmath>

namespace umbra::core
{
    Animator::Animator(std::shared_ptr<const Skeleton> skeleton, std::shared_ptr<const AnimGraph> graph, const ClipFinder &findClip)
        : skeleton_(std::move(skeleton)), graph_(std::move(graph))
    {
        // Resolve each state's clip once, and which joint each of its channels moves.
        for (const AnimState &state : graph_->states)
        {
            std::shared_ptr<const Clip> clip = findClip ? findClip(state.clip) : nullptr;
            if (clip == nullptr && missing_.empty())
                missing_ = state.clip;
            maps_.push_back(clip ? MapClip(*clip, *skeleton_) : std::vector<int>{});
            clips_.push_back(std::move(clip));
        }
        pose_ = RestPose(*skeleton_);
        ComputePalette(*skeleton_, pose_, palette_);
    }

    const std::string &Animator::State() const
    {
        static const std::string none;
        return state_ < graph_->states.size() ? graph_->states[state_].name : none;
    }

    bool Animator::Finished() const
    {
        const Clip *clip = state_ < clips_.size() ? clips_[state_].get() : nullptr;
        return clip == nullptr || time_ >= clip->duration;
    }

    bool Animator::Matches(std::size_t i) const
    {
        const AnimState &s = graph_->states[i];
        if (!s.on.empty()) // a one-shot: while it plays, or when its trigger is waiting
            return (i == state_ && !Finished()) || triggers_.contains(s.on);
        if (!s.flag.empty())
            if (const auto it = flags_.find(s.flag); it == flags_.end() || !it->second)
                return false;
        if (!s.param.empty())
        {
            const auto it = numbers_.find(s.param);
            if ((it != numbers_.end() ? it->second : 0.0F) <= s.above)
                return false;
        }
        return true;
    }

    void Animator::Pose(std::size_t state, float time, std::vector<JointPose> &out) const
    {
        if (state < clips_.size() && clips_[state] != nullptr)
            SampleClip(*clips_[state], maps_[state], time, graph_->states[state].loop, *skeleton_, out);
        else
            out = RestPose(*skeleton_);
    }

    void Animator::FireEvents(float from, float to)
    {
        // Every event whose moment lies in (from, to]. A looping clip repeats its events each lap;
        // a slow frame can cross several laps, each counted (at most 4, so a stall can't flood).
        const Clip *clip = clips_[state_].get();
        const AnimState &s = graph_->states[state_];
        if (clip == nullptr || s.events.empty())
            return;
        const bool laps = s.loop && clip->duration > 0.0F;
        const int first = laps ? static_cast<int>(std::floor(std::max(from, 0.0F) / clip->duration)) : 0;
        const int last = laps ? std::min(static_cast<int>(std::floor(to / clip->duration)), first + 3) : 0;
        for (int lap = first; lap <= last; ++lap)
            for (const AnimEvent &e : s.events)
                if (const float at = e.at + static_cast<float>(lap) * clip->duration; at > from && at <= to)
                    fired_.push_back(e);
    }

    void Animator::Update(float seconds)
    {
        // 1. The first matching state; a change fades from the current pose.
        std::size_t next = graph_->states.size();
        for (std::size_t i = 0; i < graph_->states.size() && next == graph_->states.size(); ++i)
            if (Matches(i))
                next = i;
        const bool retrigger = next < graph_->states.size() && !graph_->states[next].on.empty() && triggers_.contains(graph_->states[next].on);
        if (next != state_ || retrigger)
        {
            from_ = pose_;
            fade_ = state_ < graph_->states.size() ? graph_->fade : 0.0F;
            state_ = next;
            time_ = 0.0F;
            entered_ = true;
            if (retrigger)
                triggers_.erase(graph_->states[next].on);
        }
        // 2. Triggers not used yet wait half a second for their state, then expire (after the
        //    choice, so one slow frame cannot lose a trigger).
        for (auto it = triggers_.begin(); it != triggers_.end();)
            it = (it->second -= seconds) <= 0.0F ? triggers_.erase(it) : std::next(it);

        // 3. Advance time (rate from a number, e.g. speed / 4.5), sample, blend, palette.
        if (state_ < graph_->states.size())
        {
            const AnimState &s = graph_->states[state_];
            float rate = 1.0F;
            if (!s.rateParam.empty() && s.ratePer > 0.0F)
                if (const auto it = numbers_.find(s.rateParam); it != numbers_.end())
                    rate = std::clamp(it->second / s.ratePer, 0.5F, 2.0F);
            const float before = entered_ ? -1.0F : time_;
            time_ += seconds * rate;
            FireEvents(before, time_);
            entered_ = false;
        }
        Pose(state_, time_, pose_);
        if (fade_ > 0.0F && graph_->fade > 0.0F)
        {
            fade_ = std::max(fade_ - seconds, 0.0F);
            BlendPoses(from_, pose_, 1.0F - fade_ / graph_->fade, pose_);
        }
        ComputePalette(*skeleton_, pose_, palette_);
    }
}
