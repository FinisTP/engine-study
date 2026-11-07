#include <Interaction.hpp>

namespace umbra::gameplay
{
    world::Entity FindEntityByBody(const world::World &world, core::BodyHandle body)
    {
        // A linear scan is fine for small areas; add a body -> entity map if scenes grow large.
        world::Entity found;
        world.ForEach([&](world::Entity entity) {
            const world::Collider *collider = world.GetCollider(entity);
            if (collider != nullptr && collider->body == body)
                found = entity;
        });
        return found;
    }

    world::Entity FindInteractable(const world::World &world, const physics::PhysicsWorld &physics,
                                   const PlayerController &player)
    {
        // 1. Waist height first, then knee height, so short things (stones, chests) work too.
        for (const bool low : {false, true})
        {
            const std::optional<physics::RayHit> hit = player.InteractionQuery(physics, low);
            if (!hit)
                continue;
            // 2. The hit body's entity, or the nearest ancestor that is interactable.
            for (world::Entity entity = FindEntityByBody(world, hit->body); entity; entity = world.GetParent(entity))
            {
                if (world.GetInteractable(entity) != nullptr)
                    return entity;
            }
        }
        return {};
    }
}
