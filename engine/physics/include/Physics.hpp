#pragma once

#include <Handles.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

namespace umbra::physics
{
    using core::BodyHandle;
    using core::CharacterHandle;

    enum class ShapeKind : std::uint8_t
    {
        Box,
        Sphere,
        Capsule // vertical (along +Y), centered on the body position
    };

    struct Shape final
    {
        ShapeKind kind = ShapeKind::Box;
        glm::vec3 halfExtents{0.5F}; // Box
        float radius = 0.5F;         // Sphere, Capsule
        float halfHeight = 0.5F;     // Capsule: half the length of the straight middle part
    };

    enum class Motion : std::uint8_t
    {
        Static,    // never moves (level geometry)
        Kinematic, // moved by code, pushes dynamic bodies
        Dynamic    // moved by the simulation
    };

    struct BodyDesc final
    {
        Shape shape;
        Motion motion = Motion::Static;
        glm::vec3 position{0.0F};
        glm::quat rotation = glm::identity<glm::quat>();
        float friction = 0.5F;
        float restitution = 0.0F;
    };

    struct RayHit final
    {
        BodyHandle body;
        glm::vec3 point{0.0F};
        glm::vec3 normal{0.0F};
        float distance = 0.0F;
    };

    // A capsule that walks: slides along walls, climbs small steps, sticks to slopes.
    struct CharacterDesc final
    {
        glm::vec3 position{0.0F}; // capsule center
        float radius = 0.3F;
        float halfHeight = 0.6F;
        float maxSlopeDegrees = 50.0F;
        float stepHeight = 0.35F;
    };

    struct CharacterState final
    {
        glm::vec3 position{0.0F};
        glm::vec3 velocity{0.0F};
        bool grounded = false;
    };

    // Owns one physics simulation. Jolt is used inside; no Jolt type appears here.
    // See Physics.md for layers, stepping and ownership rules.
    class PhysicsWorld final
    {
    public:
        PhysicsWorld();
        ~PhysicsWorld();
        PhysicsWorld(const PhysicsWorld &) = delete;
        PhysicsWorld &operator=(const PhysicsWorld &) = delete;

        // Invalid handle if the shape has non-positive or non-finite sizes.
        [[nodiscard]] BodyHandle CreateBody(const BodyDesc &desc);
        bool DestroyBody(BodyHandle body);
        [[nodiscard]] bool GetBodyTransform(BodyHandle body, glm::vec3 &position, glm::quat &rotation) const;
        // Teleports static/dynamic bodies; kinematic bodies are moved so they push what they hit.
        bool SetBodyTransform(BodyHandle body, const glm::vec3 &position, const glm::quat &rotation, float deltaSeconds);
        [[nodiscard]] const Shape *GetBodyShape(BodyHandle body) const;
        [[nodiscard]] std::size_t BodyCount() const noexcept;

        // Closest hit along `direction` (normalized) within maxDistance. Characters are not hit.
        [[nodiscard]] std::optional<RayHit> Raycast(const glm::vec3 &origin, const glm::vec3 &direction, float maxDistance) const;

        [[nodiscard]] CharacterHandle CreateCharacter(const CharacterDesc &desc);
        bool DestroyCharacter(CharacterHandle character);
        // Walks with a horizontal velocity (m/s); gravity and jumping are applied here.
        bool MoveCharacter(CharacterHandle character, const glm::vec3 &horizontalVelocity, bool jump, float deltaSeconds);
        bool TeleportCharacter(CharacterHandle character, const glm::vec3 &position);
        [[nodiscard]] std::optional<CharacterState> GetCharacter(CharacterHandle character) const;
        [[nodiscard]] std::size_t CharacterCount() const noexcept;

        // Advances dynamic bodies by one fixed step. Call at a fixed rate (see Physics.md).
        void Step(float deltaSeconds);

        // Visits every body (for debug drawing).
        void ForEachBody(const std::function<void(BodyHandle, const Shape &, Motion, const glm::vec3 &, const glm::quat &)> &visit) const;

        glm::vec3 gravity{0.0F, -9.81F, 0.0F};
        float jumpSpeed = 5.0F;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
