#pragma once

#include <DrawList.hpp>
#include <Physics.hpp>
#include <World.hpp>

#include <cstddef>

namespace umbra::gameplay
{
    // Connects world::Collider components to physics bodies (ownership: engine/physics/include/Physics.md).

    // Creates a body for every collider that has none. Call after World::UpdateTransforms().
    // Returns how many colliders could not get a body (invalid sizes).
    std::size_t CreateBodies(world::World &world, physics::PhysicsWorld &physics);

    // Destroys every body created for the world's colliders.
    void DestroyBodies(world::World &world, physics::PhysicsWorld &physics);

    // Kinematic bodies follow their entity (moved by code) before the physics step.
    void PushKinematic(const world::World &world, physics::PhysicsWorld &physics, float deltaSeconds);

    // Dynamic bodies drive their (root) entity after the physics step.
    void PullDynamic(world::World &world, const physics::PhysicsWorld &physics);

    // Outlines of every body: static gray, kinematic blue, dynamic green.
    void AddDebugLines(const physics::PhysicsWorld &physics, core::DrawList &drawList);

    // Splits a world matrix into position, rotation and scale (no shear assumed).
    void Decompose(const glm::mat4 &matrix, glm::vec3 &position, glm::quat &rotation, glm::vec3 &scale);
}
