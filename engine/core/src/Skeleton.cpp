#include "Skeleton.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace umbra::core
{
    int Skeleton::Find(const std::string &name) const
    {
        // Exact name first; then without a "prefix:" namespace on either side ("mixamorig:Hips" = "Hips").
        const auto bare = [](const std::string &text) { return text.substr(text.rfind(':') + 1); };
        for (std::size_t i = 0; i < joints.size(); ++i)
            if (joints[i].name == name)
                return static_cast<int>(i);
        for (std::size_t i = 0; i < joints.size(); ++i)
            if (bare(joints[i].name) == bare(name))
                return static_cast<int>(i);
        return -1;
    }

    bool SortJoints(Skeleton &skeleton)
    {
        // Repeatedly add joints whose parent is already placed; no progress means a cycle.
        const std::size_t n = skeleton.joints.size();
        std::vector<bool> placed(n, false);
        skeleton.order.clear();
        while (skeleton.order.size() < n)
        {
            const std::size_t before = skeleton.order.size();
            for (std::size_t i = 0; i < n; ++i)
            {
                const int parent = skeleton.joints[i].parent;
                if (!placed[i] && (parent < 0 || placed[static_cast<std::size_t>(parent)]))
                {
                    placed[i] = true;
                    skeleton.order.push_back(static_cast<std::uint32_t>(i));
                }
            }
            if (skeleton.order.size() == before)
                return false;
        }
        return true;
    }

    std::string ValidateSkeleton(const Skeleton &skeleton)
    {
        if (skeleton.joints.empty() || skeleton.joints.size() > maxJoints)
            return "a skeleton needs 1.." + std::to_string(maxJoints) + " joints";
        for (const Joint &joint : skeleton.joints)
            if (joint.parent >= static_cast<int>(skeleton.joints.size()))
                return "joint '" + joint.name + "' has a parent out of range";
        if (skeleton.order.size() != skeleton.joints.size())
            return "the joint hierarchy has a cycle";
        return {};
    }

    std::vector<JointPose> RestPose(const Skeleton &skeleton)
    {
        std::vector<JointPose> pose(skeleton.joints.size());
        for (std::size_t i = 0; i < pose.size(); ++i)
            pose[i] = skeleton.joints[i].rest;
        return pose;
    }

    std::vector<glm::vec3> RestPositions(const Skeleton &skeleton, const MeshData &mesh)
    {
        // p = sum over 4 joints of weight * palette[joint] * bindPosition.
        std::vector<glm::vec3> out;
        out.reserve(mesh.vertices.size());
        std::vector<glm::mat4> palette;
        ComputePalette(skeleton, RestPose(skeleton), palette);
        for (std::size_t i = 0; i < mesh.vertices.size(); ++i)
        {
            const glm::vec4 bind(mesh.vertices[i].position, 1.0F);
            if (i >= mesh.skin.size())
            {
                out.emplace_back(bind);
                continue;
            }
            glm::vec4 p(0.0F);
            for (int k = 0; k < 4; ++k)
                if (mesh.skin[i].joints[k] < palette.size())
                    p += mesh.skin[i].weights[k] * (palette[mesh.skin[i].joints[k]] * bind);
            out.emplace_back(p);
        }
        return out;
    }

    std::vector<int> MapClip(const Clip &clip, const Skeleton &skeleton)
    {
        std::vector<int> map(clip.channels.size());
        for (std::size_t i = 0; i < map.size(); ++i)
            map[i] = skeleton.Find(clip.channels[i].joint);
        return map;
    }

    namespace
    {
        // The value of a channel at `time`: find the two keys around it and blend.
        glm::vec4 SampleChannel(const ClipChannel &channel, float time)
        {
            const std::vector<float> &t = channel.times;
            if (t.empty())
                return glm::vec4(0.0F);
            if (time <= t.front())
                return channel.values.front();
            if (time >= t.back())
                return channel.values.back();
            const std::size_t next = static_cast<std::size_t>(std::upper_bound(t.begin(), t.end(), time) - t.begin());
            const std::size_t prev = next - 1;
            if (channel.step)
                return channel.values[prev];
            const float f = (time - t[prev]) / std::max(t[next] - t[prev], 1e-6F);
            const glm::vec4 &a = channel.values[prev], &b = channel.values[next];
            if (channel.path == ClipChannel::Path::Rotation)
            {
                const glm::quat qa(a.w, a.x, a.y, a.z), qb(b.w, b.x, b.y, b.z);
                const glm::quat q = glm::slerp(qa, glm::dot(qa, qb) < 0.0F ? -qb : qb, f);
                return {q.x, q.y, q.z, q.w};
            }
            return glm::mix(a, b, f);
        }
    }

    void SampleClip(const Clip &clip, const std::vector<int> &map, float time, bool loop, const Skeleton &skeleton,
                    std::vector<JointPose> &pose)
    {
        pose = RestPose(skeleton);
        if (clip.duration > 0.0F)
            time = loop ? std::fmod(std::fmod(time, clip.duration) + clip.duration, clip.duration) : std::clamp(time, 0.0F, clip.duration);
        for (std::size_t c = 0; c < clip.channels.size() && c < map.size(); ++c)
        {
            if (map[c] < 0)
                continue;
            const glm::vec4 v = SampleChannel(clip.channels[c], time);
            JointPose &joint = pose[static_cast<std::size_t>(map[c])];
            switch (clip.channels[c].path)
            {
            case ClipChannel::Path::Position: joint.position = glm::vec3(v); break;
            case ClipChannel::Path::Rotation: joint.rotation = glm::normalize(glm::quat(v.w, v.x, v.y, v.z)); break;
            case ClipChannel::Path::Scale: joint.scale = glm::vec3(v); break;
            }
        }
    }

    void BlendPoses(const std::vector<JointPose> &a, const std::vector<JointPose> &b, float t, std::vector<JointPose> &out)
    {
        out.resize(std::min(a.size(), b.size()));
        for (std::size_t i = 0; i < out.size(); ++i)
        {
            const glm::quat qb = glm::dot(a[i].rotation, b[i].rotation) < 0.0F ? -b[i].rotation : b[i].rotation;
            out[i] = {glm::mix(a[i].position, b[i].position, t), glm::normalize(glm::slerp(a[i].rotation, qb, t)),
                      glm::mix(a[i].scale, b[i].scale, t)};
        }
    }

    void ComputePalette(const Skeleton &skeleton, const std::vector<JointPose> &pose, std::vector<glm::mat4> &palette)
    {
        // 1. Each joint's model-space matrix: its parent's (or its base) times its local T * R * S.
        std::vector<glm::mat4> model(skeleton.joints.size(), glm::mat4(1.0F));
        for (const std::uint32_t i : skeleton.order)
        {
            const Joint &joint = skeleton.joints[i];
            const JointPose &p = i < pose.size() ? pose[i] : joint.rest;
            const glm::mat4 local = glm::translate(glm::mat4(1.0F), p.position) * glm::mat4_cast(p.rotation) * glm::scale(glm::mat4(1.0F), p.scale);
            model[i] = (joint.parent >= 0 ? model[static_cast<std::size_t>(joint.parent)] * joint.base : joint.base) * local;
        }
        // 2. Skinning matrix = model-space joint * inverse bind: moves a bind-pose vertex with the joint.
        palette.resize(skeleton.joints.size());
        for (std::size_t i = 0; i < palette.size(); ++i)
            palette[i] = model[i] * skeleton.joints[i].inverseBind;
    }
}
