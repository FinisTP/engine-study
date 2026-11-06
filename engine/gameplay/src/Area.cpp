#include <Area.hpp>

#include <PhysicsBridge.hpp>

#include <glm/gtc/matrix_inverse.hpp>

#include <cmath>

namespace umbra::gameplay
{
    std::optional<SpawnPoint> FindSpawnPoint(const world::World &world, std::string_view id)
    {
        const world::Entity marker = world.Find(id);
        if (!marker)
            return std::nullopt;
        glm::vec3 position, scale;
        glm::quat rotation;
        Decompose(*world.GetWorldMatrix(marker), position, rotation, scale);
        // Facing = where the marker's -Z points, as a yaw (see PlayerController.md).
        const glm::vec3 forward = rotation * glm::vec3{0.0F, 0.0F, -1.0F};
        return SpawnPoint{position, std::atan2(-forward.x, -forward.z)};
    }

    world::Entity FindPortal(const world::World &world, const glm::vec3 &point)
    {
        // Move the point into each portal's local space; inside means every axis is within -1..1.
        world::Entity found;
        world.ForEach([&](world::Entity entity) {
            if (found || world.GetPortal(entity) == nullptr)
                return;
            const glm::vec3 local = glm::vec3(glm::inverse(*world.GetWorldMatrix(entity)) * glm::vec4(point, 1.0F));
            if (std::abs(local.x) <= 1.0F && std::abs(local.y) <= 1.0F && std::abs(local.z) <= 1.0F)
                found = entity;
        });
        return found;
    }
}
