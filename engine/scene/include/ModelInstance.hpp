#pragma once

#include <Model.hpp>
#include <World.hpp>

#include <functional>
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

    // Fills the transient handles of every renderable in the world. Returns how many stayed unresolved.
    std::size_t BindRenderables(world::World &world, const MeshResolver &resolve);
}
