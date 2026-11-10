#pragma once

#include <Skeleton.hpp>

#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Plays an entity's clips through its animation graph (S8, Skeleton.md section 4).
namespace umbra::core
{
    // Something that happens at a moment of a clip (S9, AnimGraph.md): a footstep, a sword's impact.
    struct AnimEvent final
    {
        float at = 0.0F;    // seconds into the clip
        std::string name;   // what the game hears ("step", "hit")
        std::string sound;  // optional sound to play there (asset path)
        std::string effect; // optional particle effect to spawn there (asset path)
    };

    // One state of an animation graph (`*.anim.json`, parsed by scene::ParseAnimGraph).
    struct AnimState final
    {
        std::string name;
        std::string clip;            // a clip of the model, or "models/other.glb#clip"
        bool loop = true;
        std::string flag;            // chosen while this flag is set (e.g. "dead")
        std::string param;           // chosen while this number is above `above` (e.g. "speed")
        float above = -std::numeric_limits<float>::infinity();
        std::string on;              // or: played once when this trigger fires (e.g. "attack")
        std::string rateParam;       // playback speed = param / ratePer (e.g. walk faster when moving faster)
        float ratePer = 0.0F;
        std::vector<AnimEvent> events;
    };

    struct AnimGraph final
    {
        float fade = 0.2F; // seconds to blend from one state into the next
        std::vector<AnimState> states; // first matching state wins (put death first, idle last)
    };

    class Animator final
    {
    public:
        // Returns the clip, sharing ownership of whatever holds it (nullptr = not found).
        using ClipFinder = std::function<std::shared_ptr<const Clip>(const std::string &name)>;
        Animator(std::shared_ptr<const Skeleton> skeleton, std::shared_ptr<const AnimGraph> graph, const ClipFinder &findClip);

        void SetNumber(const std::string &name, float value) { numbers_[name] = value; }
        void SetFlag(const std::string &name, bool value) { flags_[name] = value; }
        // Plays the state with `on: name` once (kept for half a second if a higher state is busy).
        void Trigger(const std::string &name) { triggers_[name] = 0.5F; }

        // Picks the state, advances time, blends from the last state, fills the palette.
        void Update(float seconds);
        [[nodiscard]] const std::vector<glm::mat4> &Palette() const { return palette_; }
        // The events the clip's time passed since the last call: each once, also at low frame rates.
        [[nodiscard]] std::vector<AnimEvent> TakeEvents() { return std::exchange(fired_, {}); }
        [[nodiscard]] const std::string &State() const;
        [[nodiscard]] float StateTime() const { return time_; }
        [[nodiscard]] bool Missing() const { return !missing_.empty(); } // a state names a clip that isn't there
        [[nodiscard]] const std::string &MissingClip() const { return missing_; }

    private:
        [[nodiscard]] bool Matches(std::size_t state) const;
        [[nodiscard]] bool Finished() const;
        void Pose(std::size_t state, float time, std::vector<JointPose> &out) const;
        void FireEvents(float from, float to);

        std::shared_ptr<const Skeleton> skeleton_;
        std::shared_ptr<const AnimGraph> graph_;
        std::vector<std::shared_ptr<const Clip>> clips_; // per state (nullptr = missing)
        std::vector<std::vector<int>> maps_;       // per state: channel -> joint
        std::map<std::string, float> numbers_;
        std::map<std::string, bool> flags_;
        std::map<std::string, float> triggers_;    // name -> seconds left
        std::size_t state_ = static_cast<std::size_t>(-1);
        float time_ = 0.0F;
        std::vector<JointPose> from_;              // the pose being faded out
        float fade_ = 0.0F;                        // seconds of fade left
        std::vector<JointPose> pose_;
        std::vector<glm::mat4> palette_;
        std::string missing_;
        std::vector<AnimEvent> fired_;
        bool entered_ = false; // the state began this update: an event at 0 s counts
    };
}
