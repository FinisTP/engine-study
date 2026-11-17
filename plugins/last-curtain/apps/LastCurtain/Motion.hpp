#pragma once

// Sprite motions (Motions.md): flat pictures moved by short keyed motions from assets/motions.json.
// Display only: the night's rules never read this.

#include <Night.hpp>

#include <glm/glm.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace umbra::night
{
    // Where a sprite is at one moment of a motion (Motions.md "A motion").
    struct MotionPose
    {
        float x = 0.0F, y = 0.0F, sx = 1.0F, sy = 1.0F, rot = 0.0F, skew = 0.0F;
        float alpha = 1.0F, shake = 0.0F, dissolve = 0.0F;
        glm::vec4 tint{1.0F, 1.0F, 1.0F, 0.0F}; // rgb mixed in by a
        std::string face = "calm";
        bool flip = false, trail = false;
    };

    // The faces a guest's sprite has a picture for.
    [[nodiscard]] bool MotionFaceKnown(const std::string &face);

    class Motions
    {
    public:
        // Refused data leaves `error` set and no motions (every sample is then the neutral pose).
        static Motions Load(const std::filesystem::path &file, std::string &error);
        static Motions Parse(const std::string &text, std::string &error);
        [[nodiscard]] MotionPose Sample(const std::string &name, float t) const;
        [[nodiscard]] bool Has(const std::string &name) const { return motions_.count(name) > 0; }
        [[nodiscard]] bool Loops(const std::string &name) const;
        [[nodiscard]] float Seconds(const std::string &name) const;
        [[nodiscard]] std::vector<std::string> Names() const;

    private:
        struct Key
        {
            float t = 0.0F;
            std::map<std::string, float> values; // x, y, ..., and tint as tint0..tint3
        };
        struct Motion
        {
            float seconds = 1.0F;
            bool loop = false, flip = false, trail = false;
            std::vector<Key> keys;
            std::vector<std::pair<float, std::string>> faces;
        };
        std::map<std::string, Motion> motions_;
    };

    // One guest's sprite: a base loop (idle, walk, ghost, fear) and one-shots over it.
    class SpriteState
    {
    public:
        void Base(const std::string &name); // the loop to rest in; a new one restarts its clock
        void Play(const std::string &name); // a one-shot, then back to the base
        void Hold(const std::string &name); // a one-shot that stays on its last frame (down)
        void Release();                     // end a held one-shot
        void Update(float dt, const Motions &motions);
        [[nodiscard]] const std::string &Motion() const { return shot_.empty() ? base_ : shot_; }
        [[nodiscard]] MotionPose Pose(const Motions &motions, float back = 0.0F) const; // back: seconds earlier (a trail)
        float facing = 1.0F; // +1 right, -1 left on screen

    private:
        std::string base_ = "idle", shot_;
        float baseT_ = 0.0F, shotT_ = 0.0F;
        bool hold_ = false;
    };

    // The model matrix of the 1 m quad (centred, facing +Z, v down) for a sprite whose feet stand at
    // `feet`, upright, its picture `height` metres tall and `aspect` wide per tall (Motions.md "The transform").
    [[nodiscard]] glm::mat4 SpriteModel(const MotionPose &pose, glm::vec3 feet, glm::vec3 right, float height, float aspect, float facing, float time);

    // Who moves how when this happens (Motions.md "Which moment plays which motion").
    [[nodiscard]] std::vector<std::pair<int, std::string>> MotionsFor(const LogEntry &entry);
}
