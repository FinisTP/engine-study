#pragma once

#include <MeshData.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace umbra::assets
{
    // One glTF node: a transform relative to its parent plus the meshes it draws.
    struct ModelNode final
    {
        std::string name;
        std::int32_t parent = -1; // index into Model::nodes, -1 = root
        glm::vec3 position{0.0F};
        glm::quat rotation = glm::identity<glm::quat>();
        glm::vec3 scale{1.0F};
        std::vector<std::uint32_t> meshes; // indices into Model::meshes
    };

    // One glTF primitive (an indexed triangle list with one material).
    struct ModelMesh final
    {
        core::MeshData data;
        std::int32_t texture = -1; // index into Model::textures, -1 = none
    };

    // Engine-side copy of a glTF file: hierarchy, meshes and base-color textures.
    // Parents always come before their children in `nodes`.
    struct Model final
    {
        std::vector<ModelNode> nodes;
        std::vector<ModelMesh> meshes;
        std::vector<core::TextureData> textures;
    };

    // True if both models have the same nodes (names, parents, meshes per node).
    // Then only mesh/texture *contents* differ, and entities built from `a` still fit `b`.
    [[nodiscard]] inline bool SameStructure(const Model &a, const Model &b)
    {
        if (a.nodes.size() != b.nodes.size() || a.meshes.size() != b.meshes.size())
            return false;
        for (std::size_t i = 0; i < a.nodes.size(); ++i)
        {
            const ModelNode &x = a.nodes[i];
            const ModelNode &y = b.nodes[i];
            if (x.name != y.name || x.parent != y.parent || x.meshes != y.meshes)
                return false;
        }
        return true;
    }
}
