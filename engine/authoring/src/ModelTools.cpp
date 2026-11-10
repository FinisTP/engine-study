#include <ModelTools.hpp>

#include <GltfImporter.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace umbra::authoring
{
    namespace
    {
        float Round(float value)
        {
            return std::round(value * 1000.0F) / 1000.0F;
        }

        // A mesh's points as drawn at rest: skinned by the rest pose if the model is rigged.
        std::vector<glm::vec3> Positions(const assets::Model &model, std::uint32_t mesh)
        {
            if (model.skeleton)
                return core::RestPositions(*model.skeleton, model.meshes[mesh].data);
            std::vector<glm::vec3> points;
            for (const core::Vertex &v : model.meshes[mesh].data.vertices)
                points.push_back(v.position);
            return points;
        }
    }

    bool InspectModel(const std::filesystem::path &file, ModelInfo &info, std::vector<core::Diagnostic> &diagnostics)
    {
        const assets::ImportResult imported = assets::ImportGltf(file);
        for (const std::string &error : imported.errors)
            diagnostics.push_back({core::Severity::Error, "model.import", file.string(), "", error});
        if (!imported.Ok())
            return false;
        const assets::Model &model = imported.model;
        info = {};
        for (const auto &mesh : model.meshes)
            info.triangles += mesh.data.indices.size() / 3;
        info.meshes = model.meshes.size();
        info.joints = model.skeleton ? model.skeleton->joints.size() : 0;
        for (const core::Clip &clip : model.clips)
            info.clips.push_back(clip.name);
        for (const auto &texture : model.textures)
            info.textureSize = std::max({info.textureSize, texture.width, texture.height});

        // Bounds in model space: each node's world matrix (parents come first) applied to its meshes.
        std::vector<glm::mat4> world(model.nodes.size(), glm::mat4(1.0F));
        bool first = true;
        for (std::size_t i = 0; i < model.nodes.size(); ++i)
        {
            const assets::ModelNode &node = model.nodes[i];
            const glm::mat4 local = glm::translate(glm::mat4(1.0F), node.position) * glm::mat4_cast(node.rotation) * glm::scale(glm::mat4(1.0F), node.scale);
            world[i] = node.parent >= 0 ? world[static_cast<std::size_t>(node.parent)] * local : local;
            for (const std::uint32_t m : node.meshes)
                for (const glm::vec3 &position : Positions(model, m))
                {
                    // A rigged mesh is drawn by its joints, not its node (Skeleton.md).
                    const glm::vec3 p = model.skeleton ? position : glm::vec3(world[i] * glm::vec4(position, 1.0F));
                    info.min = first ? p : glm::min(info.min, p);
                    info.max = first ? p : glm::max(info.max, p);
                    first = false;
                }
        }
        return true;
    }

    nlohmann::ordered_json FittedCollider(const ModelInfo &info, glm::vec3 scale)
    {
        // A box around the model's bounds in meters: half its size times the entity's scale.
        // The box is centered on the entity, so it fits models centered on their origin (TRELLIS.2 is).
        const glm::vec3 half = (info.max - info.min) * 0.5F * glm::abs(scale);
        return {{"shape", "box"}, {"motion", "static"}, {"halfExtents", {Round(half.x), Round(half.y), Round(half.z)}}};
    }
}
