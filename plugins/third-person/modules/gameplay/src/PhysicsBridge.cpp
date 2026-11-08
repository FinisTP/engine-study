#include <PhysicsBridge.hpp>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>

namespace umbra::gameplay
{
    namespace
    {
        // Collider sizes of 0 are taken from the entity's world scale. The sample cube spans
        // -1..1, so a box's half extents equal its scale.
        physics::Shape ToShape(const world::Collider &c, const glm::vec3 &scale)
        {
            physics::Shape shape;
            switch (c.shape)
            {
            case world::ColliderShape::Box:
                shape.kind = physics::ShapeKind::Box;
                shape.halfExtents = c.halfExtents == glm::vec3{0.0F} ? scale : c.halfExtents;
                break;
            case world::ColliderShape::Sphere:
                shape.kind = physics::ShapeKind::Sphere;
                shape.radius = c.radius > 0.0F ? c.radius : std::max({scale.x, scale.y, scale.z});
                break;
            case world::ColliderShape::Capsule:
                shape.kind = physics::ShapeKind::Capsule;
                shape.radius = c.radius > 0.0F ? c.radius : std::max(scale.x, scale.z);
                shape.halfHeight = c.halfHeight > 0.0F ? c.halfHeight : std::max(scale.y - shape.radius, 0.01F);
                break;
            }
            return shape;
        }

        physics::Motion ToMotion(world::ColliderMotion motion)
        {
            return motion == world::ColliderMotion::Dynamic     ? physics::Motion::Dynamic
                   : motion == world::ColliderMotion::Kinematic ? physics::Motion::Kinematic
                                                                : physics::Motion::Static;
        }

        void AddCircle(core::DrawList &list, const glm::vec3 &center, const glm::quat &rotation, float radius, int axis, glm::vec4 color)
        {
            constexpr int segments = 16;
            glm::vec3 previous{};
            for (int i = 0; i <= segments; ++i)
            {
                // Circle in the plane perpendicular to `axis` (0 = X, 1 = Y, 2 = Z).
                const float a = glm::two_pi<float>() * static_cast<float>(i) / segments;
                glm::vec3 local{0.0F};
                local[(axis + 1) % 3] = radius * std::cos(a);
                local[(axis + 2) % 3] = radius * std::sin(a);
                const glm::vec3 point = center + rotation * local;
                if (i > 0)
                    list.lines.push_back({previous, point, color});
                previous = point;
            }
        }
    }

    void Decompose(const glm::mat4 &matrix, glm::vec3 &position, glm::quat &rotation, glm::vec3 &scale)
    {
        // Column lengths are the scale; the normalized columns form the rotation.
        const glm::vec3 x{matrix[0]}, y{matrix[1]}, z{matrix[2]};
        position = glm::vec3{matrix[3]};
        scale = {glm::length(x), glm::length(y), glm::length(z)};
        rotation = glm::normalize(glm::quat_cast(glm::mat3{x / scale.x, y / scale.y, z / scale.z}));
    }

    std::size_t CreateBodies(world::World &world, physics::PhysicsWorld &physics)
    {
        std::size_t failed = 0;
        world.ForEach([&](world::Entity entity) {
            world::Collider *collider = world.GetCollider(entity);
            if (collider == nullptr || physics.GetBodyShape(collider->body) != nullptr)
                return;
            physics::BodyDesc desc;
            glm::vec3 scale;
            Decompose(*world.GetWorldMatrix(entity), desc.position, desc.rotation, scale);
            desc.shape = ToShape(*collider, scale);
            desc.motion = ToMotion(collider->motion);
            collider->body = physics.CreateBody(desc);
            if (!collider->body)
                ++failed;
        });
        return failed;
    }

    void DestroyBodies(world::World &world, physics::PhysicsWorld &physics)
    {
        world.ForEach([&](world::Entity entity) {
            if (world::Collider *collider = world.GetCollider(entity))
            {
                physics.DestroyBody(collider->body);
                collider->body = {};
            }
        });
    }

    void PushKinematic(const world::World &world, physics::PhysicsWorld &physics, float deltaSeconds)
    {
        world.ForEach([&](world::Entity entity) {
            const world::Collider *collider = world.GetCollider(entity);
            if (collider == nullptr || collider->motion != world::ColliderMotion::Kinematic)
                return;
            glm::vec3 position, scale;
            glm::quat rotation;
            Decompose(*world.GetWorldMatrix(entity), position, rotation, scale);
            physics.SetBodyTransform(collider->body, position, rotation, deltaSeconds);
        });
    }

    void PullDynamic(world::World &world, const physics::PhysicsWorld &physics)
    {
        world.ForEach([&](world::Entity entity) {
            const world::Collider *collider = world.GetCollider(entity);
            if (collider == nullptr || collider->motion != world::ColliderMotion::Dynamic || world.GetParent(entity))
                return;
            // Keep the entity's scale; take position and rotation from the simulation.
            world::Transform transform = *world.GetLocalTransform(entity);
            if (physics.GetBodyTransform(collider->body, transform.position, transform.rotation))
                world.SetLocalTransform(entity, transform);
        });
    }

    void AddDebugLines(const physics::PhysicsWorld &physics, core::DrawList &drawList)
    {
        physics.ForEachBody([&](physics::BodyHandle, const physics::Shape &shape, physics::Motion motion, const glm::vec3 &p, const glm::quat &r) {
            const glm::vec4 color = motion == physics::Motion::Static    ? glm::vec4{0.6F, 0.6F, 0.6F, 1.0F}
                                    : motion == physics::Motion::Dynamic ? glm::vec4{0.3F, 1.0F, 0.3F, 1.0F}
                                                                         : glm::vec4{0.3F, 0.6F, 1.0F, 1.0F};
            if (shape.kind == physics::ShapeKind::Box)
            {
                // 8 corners, 12 edges: edges join corners that differ in exactly one axis.
                std::array<glm::vec3, 8> corners;
                for (int i = 0; i < 8; ++i)
                {
                    const glm::vec3 sign{(i & 1) ? 1.0F : -1.0F, (i & 2) ? 1.0F : -1.0F, (i & 4) ? 1.0F : -1.0F};
                    corners[static_cast<std::size_t>(i)] = p + r * (sign * shape.halfExtents);
                }
                for (int i = 0; i < 8; ++i)
                    for (int bit = 1; bit < 8; bit <<= 1)
                        if ((i & bit) == 0)
                            drawList.lines.push_back({corners[static_cast<std::size_t>(i)], corners[static_cast<std::size_t>(i | bit)], color});
            }
            else if (shape.kind == physics::ShapeKind::Sphere)
            {
                for (int axis = 0; axis < 3; ++axis)
                    AddCircle(drawList, p, r, shape.radius, axis, color);
            }
            else
            {
                // Capsule: rings at both ends of the straight part, joined by 4 lines.
                const glm::vec3 up = r * glm::vec3{0.0F, shape.halfHeight, 0.0F};
                AddCircle(drawList, p + up, r, shape.radius, 1, color);
                AddCircle(drawList, p - up, r, shape.radius, 1, color);
                for (const glm::vec3 side : {glm::vec3{1, 0, 0}, glm::vec3{-1, 0, 0}, glm::vec3{0, 0, 1}, glm::vec3{0, 0, -1}})
                {
                    const glm::vec3 offset = r * (side * shape.radius);
                    drawList.lines.push_back({p + up + offset, p - up + offset, color});
                }
            }
        });
    }
}
