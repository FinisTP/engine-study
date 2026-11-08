#include <ModelInstance.hpp>

namespace umbra::scene
{
    namespace
    {
        // Returns `base`, or `base#<n>` for the first free n, so ids never clash.
        std::string UniqueId(const world::World &world, const std::string &base)
        {
            std::string id = base;
            for (int n = 1; world.Find(id); ++n)
                id = base + "#" + std::to_string(n);
            return id;
        }
    }

    std::vector<world::Entity> InstantiateModel(world::World &world, const assets::Model &model, const std::string &modelPath,
                                                const std::string &idPrefix, world::Entity parent)
    {
        std::vector<world::Entity> entities;
        entities.reserve(model.nodes.size());
        for (std::size_t i = 0; i < model.nodes.size(); ++i)
        {
            const assets::ModelNode &node = model.nodes[i];

            // 1. Entity with the node's local transform, under its parent (parents come first).
            const std::string name = node.name.empty() ? "node" + std::to_string(i) : node.name;
            const world::Entity entity = world.Create(UniqueId(world, idPrefix + "/" + name), name);
            world.SetLocalTransform(entity, world::Transform{node.position, node.rotation, node.scale});
            world.SetParent(entity, node.parent >= 0 ? entities[static_cast<std::size_t>(node.parent)] : parent);

            // 2. First mesh on the entity itself, extra meshes on child entities.
            for (std::size_t k = 0; k < node.meshes.size(); ++k)
            {
                world::Entity target = entity;
                if (k > 0)
                {
                    target = world.Create(UniqueId(world, *world.GetId(entity) + "#" + std::to_string(k)), name);
                    world.SetParent(target, entity);
                }
                world.SetRenderable(target, world::Renderable{modelPath, node.meshes[k]});
            }
            entities.push_back(entity);
        }
        return entities;
    }

    std::size_t BindRenderables(world::World &world, const MeshResolver &resolve, const MaterialResolver &materials)
    {
        std::size_t unresolved = 0;
        world.ForEach([&](world::Entity entity) {
            world::Renderable *renderable = world.GetRenderable(entity);
            if (renderable == nullptr)
                return;
            const auto [mesh, texture] = resolve(renderable->model, renderable->mesh);
            renderable->meshHandle = mesh;
            renderable->textureHandle = texture;
            renderable->tint = glm::vec4{1.0F};
            renderable->metersPerRepeat = 0.0F;
            renderable->surface.clear();
            if (!renderable->material.empty() && materials)
                if (const std::optional<MaterialBinding> binding = materials(renderable->material))
                {
                    renderable->textureHandle = binding->texture;
                    renderable->tint = binding->tint;
                    renderable->metersPerRepeat = binding->metersPerRepeat;
                    renderable->surface = binding->surface;
                }
            if (!mesh)
                ++unresolved;
        });
        return unresolved;
    }
}
