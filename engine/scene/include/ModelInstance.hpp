#pragma once

#include <Model.hpp>
#include <World.hpp>

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace umbra::scene
{
    // Creates one entity per model node, keeping the node hierarchy.
    // - ids are "<idPrefix>/<node name>" ("node<i>" when unnamed, "#<i>" appended on clashes)
    // - a node's first mesh goes on its entity; extra meshes become child entities "<id>#<k>"
    // - Renderable.model = modelPath, Renderable.mesh = mesh index; GPU handles are left empty
    // Returns the entity for each node (same order as model.nodes). Roots get `parent`.
    std::vector<world::Entity> InstantiateModel(world::World &world, const assets::Model &model, const std::string &modelPath,
                                                const std::string &idPrefix, world::Entity parent = {});

    // Looks up GPU handles for a (model path, mesh index) pair; returns invalid handles if unknown.
    using MeshResolver = std::function<std::pair<core::MeshHandle, core::TextureHandle>(const std::string &model, std::uint32_t mesh)>;

    // What a material file turns into on the GPU (see engine/assets/include/Material.md).
    struct MaterialBinding final
    {
        core::TextureHandle texture;
        glm::vec4 tint{1.0F};
        float metersPerRepeat = 0.0F; // 0 = the model's own UVs
    };
    // Looks up a material path; nullopt if it can't be loaded (the model's texture is kept).
    using MaterialResolver = std::function<std::optional<MaterialBinding>(const std::string &material)>;

    // Fills the transient handles of every renderable in the world. Returns how many stayed unresolved.
    // Renderables with a material get its texture, tint and texture scale instead of the model's.
    std::size_t BindRenderables(world::World &world, const MeshResolver &resolve, const MaterialResolver &materials = {});
}
