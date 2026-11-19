#include "Motion.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>

namespace umbra::night
{
    namespace
    {
        constexpr std::array fields{"x", "y", "sx", "sy", "rot", "skew", "alpha", "shake", "dissolve", "tint0", "tint1", "tint2", "tint3"};

        float Smooth(float a) { return a * a * (3.0F - 2.0F * a); }

        void Set(MotionPose &p, const std::string &field, float v)
        {
            if (field == "x") p.x = v;
            else if (field == "y") p.y = v;
            else if (field == "sx") p.sx = v;
            else if (field == "sy") p.sy = v;
            else if (field == "rot") p.rot = v;
            else if (field == "skew") p.skew = v;
            else if (field == "alpha") p.alpha = std::clamp(v, 0.0F, 1.0F);
            else if (field == "shake") p.shake = v;
            else if (field == "dissolve") p.dissolve = std::clamp(v, 0.0F, 1.0F);
            else p.tint[field.back() - '0'] = v; // tint0..tint3
        }
    }

    bool MotionFaceKnown(const std::string &face)
    {
        for (const char *f : {"calm", "angry", "neardead", "happy", "scheming", "afraid", "talk"})
            if (face == f)
                return true;
        return false;
    }

    Motions Motions::Load(const std::filesystem::path &file, std::string &error)
    {
        std::ifstream in(file);
        if (!in)
        {
            error = file.string() + ": cannot be read";
            return {};
        }
        std::stringstream text;
        text << in.rdbuf();
        return Parse(text.str(), error);
    }

    Motions Motions::Parse(const std::string &text, std::string &error)
    {
        const nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
        if (!j.is_object())
        {
            error = "motions: not a JSON object";
            return {};
        }
        Motions out;
        for (const auto &[name, m] : j.items())
        {
            if (name.starts_with("_"))
                continue;
            // 1. A length, keys in time order inside it, and known faces.
            Motion motion;
            motion.seconds = m.value("seconds", 0.0F);
            motion.loop = m.value("loop", false), motion.flip = m.value("flip", false), motion.trail = m.value("trail", false);
            const nlohmann::json keys = m.value("keys", nlohmann::json::array());
            if (motion.seconds <= 0.0F || !keys.is_array() || keys.empty())
            {
                error = "motions: " + name + " needs seconds above 0 and keys";
                return {};
            }
            for (const nlohmann::json &k : keys)
            {
                Key key;
                key.t = k.value("t", 0.0F);
                if (!motion.keys.empty() && key.t < motion.keys.back().t)
                {
                    error = "motions: " + name + "'s keys are out of time order";
                    return {};
                }
                for (const char *f : fields)
                    if (k.contains(f))
                        key.values[f] = k[f].get<float>();
                if (k.contains("tint") && k["tint"].is_array() && k["tint"].size() == 4)
                    for (int c = 0; c < 4; ++c)
                        key.values["tint" + std::to_string(c)] = k["tint"][static_cast<std::size_t>(c)].get<float>();
                motion.keys.push_back(std::move(key));
            }
            for (const nlohmann::json &f : m.value("faces", nlohmann::json::array()))
            {
                if (!f.is_array() || f.size() != 2 || !MotionFaceKnown(f[1].get<std::string>()))
                {
                    error = "motions: " + name + " names an unknown face";
                    return {};
                }
                motion.faces.emplace_back(f[0].get<float>(), f[1].get<std::string>());
            }
            out.motions_[name] = std::move(motion);
        }
        return out;
    }

    MotionPose Motions::Sample(const std::string &name, float t) const
    {
        MotionPose pose;
        const auto found = motions_.find(name);
        if (found == motions_.end())
            return pose;
        const Motion &m = found->second;
        // 1. Loop or hold the time.
        t = m.loop ? std::fmod(std::max(t, 0.0F), m.seconds) : std::clamp(t, 0.0F, m.seconds);
        pose.flip = m.flip, pose.trail = m.trail;
        // 2. Each field: its last key at or before t, eased toward its next key (Motions.md).
        for (const char *f : fields)
        {
            const Key *prev = nullptr, *next = nullptr;
            for (const Key &k : m.keys)
                if (k.values.count(f) > 0)
                {
                    if (k.t > t)
                    {
                        next = &k;
                        break;
                    }
                    prev = &k;
                }
            if (prev != nullptr && next != nullptr)
            {
                const float from = prev->values.at(f), a = Smooth((t - prev->t) / std::max(next->t - prev->t, 1e-6F));
                Set(pose, f, from + (next->values.at(f) - from) * a);
            }
            else if (prev != nullptr || next != nullptr) // after the last key or before the first: hold it
                Set(pose, f, (prev != nullptr ? prev : next)->values.at(f));
        }
        // 3. The face: the last switch at or before t.
        for (const auto &[at, face] : m.faces)
            if (at <= t)
                pose.face = face;
        return pose;
    }

    bool Motions::Loops(const std::string &name) const
    {
        const auto found = motions_.find(name);
        return found != motions_.end() && found->second.loop;
    }

    float Motions::Seconds(const std::string &name) const
    {
        const auto found = motions_.find(name);
        return found == motions_.end() ? 0.0F : found->second.seconds;
    }

    std::vector<std::string> Motions::Names() const
    {
        std::vector<std::string> names;
        for (const auto &[name, m] : motions_)
            names.push_back(name);
        return names;
    }

    void SpriteState::Base(const std::string &name)
    {
        if (name != base_)
            base_ = name, baseT_ = 0.0F;
    }

    void SpriteState::Play(const std::string &name) { shot_ = name, shotT_ = 0.0F, hold_ = false; }

    void SpriteState::Hold(const std::string &name) { shot_ = name, shotT_ = 0.0F, hold_ = true; }

    void SpriteState::Release() { shot_.clear(), hold_ = false; }

    void SpriteState::Update(float dt, const Motions &motions)
    {
        baseT_ += dt;
        if (shot_.empty())
            return;
        shotT_ += dt;
        // A finished one-shot gives way to the base, unless it is held on its last frame.
        if (!hold_ && shotT_ >= motions.Seconds(shot_))
            shot_.clear();
    }

    MotionPose SpriteState::Pose(const Motions &motions, float back) const
    {
        return shot_.empty() ? motions.Sample(base_, baseT_ - back) : motions.Sample(shot_, std::max(shotT_ - back, 0.0F));
    }

    glm::mat4 SpriteModel(const MotionPose &pose, glm::vec3 feet, glm::vec3 right, float height, float aspect, float facing, float time, glm::vec3 up)
    {
        // 1. Quad (-0.5..0.5) -> feet-local metres, y up: the picture's bottom middle at the origin.
        const glm::mat3 toLocal{glm::vec3{aspect * height, 0, 0}, glm::vec3{0, height, 0}, glm::vec3{0, 0.5F * height, 1}};
        // 2. Scale about the feet, skew, then lean (+ = the top forward); columns are glm's.
        const float r = glm::radians(pose.rot);
        const glm::mat3 scale{glm::vec3{pose.sx, 0, 0}, glm::vec3{0, pose.sy, 0}, glm::vec3{0, 0, 1}};
        const glm::mat3 skew{glm::vec3{1, 0, 0}, glm::vec3{pose.skew, 1, 0}, glm::vec3{0, 0, 1}};
        const glm::mat3 lean{glm::vec3{std::cos(r), -std::sin(r), 0}, glm::vec3{std::sin(r), std::cos(r), 0}, glm::vec3{0, 0, 1}};
        // 3. The motion's offset and shake, in sprite heights.
        const float shake = pose.shake * height * std::sin(time * 70.0F);
        const glm::mat3 move{glm::vec3{1, 0, 0}, glm::vec3{0, 1, 0}, glm::vec3{pose.x * height + shake, pose.y * height, 1}};
        // 4. Facing left: the same motion seen in a mirror, on both sides (winding kept; Motions.md).
        const glm::mat3 mirror{glm::vec3{facing < 0.0F ? -1.0F : 1.0F, 0, 0}, glm::vec3{0, 1, 0}, glm::vec3{0, 0, 1}};
        const glm::mat3 a = mirror * move * lean * skew * scale * mirror * toLocal;
        // 5. The sprite's plane in the world: right and up, its normal toward the camera.
        const glm::vec3 normal = glm::cross(right, up);
        glm::mat4 m{1.0F};
        m[0] = glm::vec4(right * a[0][0] + up * a[0][1], 0.0F);
        m[1] = glm::vec4(right * a[1][0] + up * a[1][1], 0.0F);
        m[2] = glm::vec4(normal, 0.0F);
        m[3] = glm::vec4(feet + right * a[2][0] + up * a[2][1], 1.0F);
        return m;
    }

    std::vector<std::pair<int, std::string>> MotionsFor(const LogEntry &e)
    {
        switch (e.kind)
        {
        case LogKind::Hit: // a monster striker (-1) has no sprite
            if (e.who >= 0)
                return {{e.who, "attack"}, {e.other, "hit"}};
            return {{e.other, "hit"}};
        case LogKind::Check: return {{e.who, e.a + e.other >= e.b ? "pass" : "fail"}}; // a = die, other = stat, b = target
        case LogKind::Effect: return {{e.who, e.b < 0 ? "hit" : "use"}};
        case LogKind::Item: return {{e.who, "pickup"}};
        case LogKind::Persuade: return {{e.who, "talk"}, {e.other, "talk"}};
        case LogKind::Down: return {{e.who, "down"}};
        case LogKind::Died: return {{e.who, "die"}};
        case LogKind::Woke:
        case LogKind::Rose: return {{e.who, "revive"}};
        case LogKind::Level: return {{e.who, "levelup"}};
        case LogKind::Power:
        case LogKind::Cleared: return {{e.who, "power"}};
        default: return {};
        }
    }
}
