#pragma once

#include <Physics.hpp>
#include <PlayerController.hpp>
#include <World.hpp>

namespace umbra::gameplay
{
    // The entity whose collider owns `body`, or an invalid Entity.
    [[nodiscard]] world::Entity FindEntityByBody(const world::World &world, core::BodyHandle body);

    // The interactable the player is facing within reach, or an invalid Entity.
    // A hit on a child counts for the nearest parent that is interactable.
    [[nodiscard]] world::Entity FindInteractable(const world::World &world, const physics::PhysicsWorld &physics,
                                                 const PlayerController &player);
}
