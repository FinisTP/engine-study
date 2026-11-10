#pragma once

#include "MeshData.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <string>
#include <vector>

// Skeletal animation data and math (S8). The math: Skeleton.md.
namespace umbra::core
{
    // A joint's local transform (relative to its parent joint).
    struct JointPose final
    {
        glm::vec3 position{0.0F};
        glm::quat rotation = glm::identity<glm::quat>();
        glm::vec3 scale{1.0F};
    };

    struct Joint final
    {
        std::string name;
        std::int32_t parent = -1;   // nearest ancestor joint, -1 = none
        glm::mat4 base{1.0F};       // fixed nodes between that ancestor (or the model root) and this joint
        glm::mat4 inverseBind{1.0F};// model space at bind time -> this joint's space
        JointPose rest;             // pose when no clip moves it
    };

    // Joints in glTF skin order (the order JOINTS_0 indexes); `order` lists them parents first.
    struct Skeleton final
    {
        std::vector<Joint> joints;
        std::vector<std::uint32_t> order;
        // A joint by name (a "mixamorig:" style prefix is ignored if needed); -1 if none.
        [[nodiscard]] int Find(const std::string &name) const;
    };

    // One animated property of one joint: key times and values (vec3 in xyz, or a quaternion xyzw).
    struct ClipChannel final
    {
        enum class Path : std::uint8_t { Position, Rotation, Scale };
        std::string joint; // by name, so clips from other files with the same joint names fit
        Path path = Path::Rotation;
        bool step = false; // true: hold each key (no blending between keys)
        std::vector<float> times;
        std::vector<glm::vec4> values;
    };

    struct Clip final
    {
        std::string name;
        float duration = 0.0F;
        std::vector<ClipChannel> channels;
    };

    // Up to this many joints per skinned mesh (one byte per joint index on the GPU is not used:
    // indices are 16-bit, but palettes are capped to keep per-frame upload small).
    inline constexpr std::size_t maxJoints = 256;

    // "" if the skeleton is usable, otherwise why not (parents in range, no cycles, size).
    [[nodiscard]] std::string ValidateSkeleton(const Skeleton &skeleton);
    // Fills `order` (parents before children). Returns false on a cycle.
    bool SortJoints(Skeleton &skeleton);

    // The clip's pose at `time` (seconds; wrapped when `loop`, else held at the end). Joints the clip
    // does not move keep their rest pose. `map[i]` = the joint index of channel i (-1 = not in this skeleton).
    [[nodiscard]] std::vector<int> MapClip(const Clip &clip, const Skeleton &skeleton);
    void SampleClip(const Clip &clip, const std::vector<int> &map, float time, bool loop, const Skeleton &skeleton,
                    std::vector<JointPose> &pose);
    // a + (b - a) * t per joint (rotations by shortest-path slerp).
    void BlendPoses(const std::vector<JointPose> &a, const std::vector<JointPose> &b, float t, std::vector<JointPose> &out);
    // The skinning matrices: model space of each joint times its inverse bind matrix.
    void ComputePalette(const Skeleton &skeleton, const std::vector<JointPose> &pose, std::vector<glm::mat4> &palette);
    [[nodiscard]] std::vector<JointPose> RestPose(const Skeleton &skeleton);
    // Where each vertex of a skinned mesh is drawn at rest (what the GPU does, on the CPU): for
    // bounds and feet height. A mesh without skin data gives its own positions.
    [[nodiscard]] std::vector<glm::vec3> RestPositions(const Skeleton &skeleton, const MeshData &mesh);
}
