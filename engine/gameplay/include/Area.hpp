#pragma once

#include <World.hpp>

#include <glm/vec3.hpp>

#include <optional>
#include <string_view>

namespace umbra::gameplay
{
    struct SpawnPoint final
    {
        glm::vec3 feet{0.0F};
        float yaw = 0.0F;
    };

    // The entity with this id marks a spawn: its world position is the feet, its rotation the facing.
    // Call after World::UpdateTransforms(). Nothing if there is no such entity.
    [[nodiscard]] std::optional<SpawnPoint> FindSpawnPoint(const world::World &world, std::string_view id);

    // The portal whose box contains `point`, or an invalid Entity. A portal's box is its
    // transform applied to the -1..1 cube (the same box the sample cube mesh draws).
    [[nodiscard]] world::Entity FindPortal(const world::World &world, const glm::vec3 &point);
}
