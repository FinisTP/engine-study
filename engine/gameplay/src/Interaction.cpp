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
        const std::optional<physics::RayHit> hit = player.InteractionQuery(physics);
        if (!hit)
            return {};
        for (world::Entity entity = FindEntityByBody(world, hit->body); entity; entity = world.GetParent(entity))
        {
            if (world.GetInteractable(entity) != nullptr)
                return entity;
        }
        return {};
    }
}
