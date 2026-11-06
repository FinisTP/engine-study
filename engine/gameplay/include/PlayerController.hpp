#pragma once

#include <DrawList.hpp>
#include <Physics.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <optional>

namespace umbra::gameplay
{
    // One fixed step of player intent. The app fills it from keyboard/mouse (or a test from code).
    struct PlayerInput final
    {
        glm::vec2 move{0.0F}; // x = right, y = forward, each -1..1 (camera-relative)
        bool sprint = false;
        bool jump = false;
    };

    struct PlayerSettings final
    {
        float walkSpeed = 3.5F;
        float sprintSpeed = 6.5F;
        float turnSpeed = 12.0F;      // how fast the body turns toward the move direction (rad/s scale)
        float radius = 0.3F;          // capsule
        float halfHeight = 0.6F;
        float cameraDistance = 4.5F;
        float cameraHeight = 1.0F;    // orbit pivot above the capsule center
        float minPitch = -1.2F;       // radians, looking down
        float maxPitch = 0.5F;        // radians, looking up
        float interactDistance = 2.0F;
    };

    // Third-person player: a physics character moved relative to an orbit camera.
    // See PlayerController.md for the angle conventions and camera collision.
    class PlayerController final
    {
    public:
        explicit PlayerController(PlayerSettings settings = {}) : settings_(settings) {}

        // position = where the capsule's feet touch the ground.
        bool Spawn(physics::PhysicsWorld &physics, const glm::vec3 &feet, float yaw);
        void Despawn(physics::PhysicsWorld &physics);
        // Forgets the character without touching physics (use when its PhysicsWorld was destroyed).
        void Reset() noexcept { character_ = {}; }
        [[nodiscard]] bool IsSpawned() const noexcept { return static_cast<bool>(character_); }

        // Camera look, once per frame (mouse). Pitch is clamped.
        void Look(float yawDelta, float pitchDelta);
        // Movement, once per fixed physics step.
        void Update(physics::PhysicsWorld &physics, const PlayerInput &input, float deltaSeconds);

        // Orbit camera behind the player, pulled closer when something is in the way.
        [[nodiscard]] core::Camera GetCamera(const physics::PhysicsWorld &physics) const;
        // What the player is facing within interactDistance (ray from waist height).
        [[nodiscard]] std::optional<physics::RayHit> InteractionQuery(const physics::PhysicsWorld &physics) const;

        [[nodiscard]] glm::vec3 GetFeet(const physics::PhysicsWorld &physics) const;
        [[nodiscard]] glm::vec3 GetVelocity(const physics::PhysicsWorld &physics) const;
        [[nodiscard]] bool IsGrounded(const physics::PhysicsWorld &physics) const;
        [[nodiscard]] float GetFacingYaw() const noexcept { return facingYaw_; }
        [[nodiscard]] float GetCameraYaw() const noexcept { return cameraYaw_; }
        [[nodiscard]] float GetCameraPitch() const noexcept { return cameraPitch_; }
        [[nodiscard]] const PlayerSettings &GetSettings() const noexcept { return settings_; }
        void AddDebugLines(const physics::PhysicsWorld &physics, core::DrawList &drawList) const;

    private:
        [[nodiscard]] glm::vec3 Center(const physics::PhysicsWorld &physics) const;

        PlayerSettings settings_;
        physics::CharacterHandle character_;
        float cameraYaw_ = 0.0F;   // 0 = camera looks along -Z
        float cameraPitch_ = -0.3F;
        float facingYaw_ = 0.0F;   // where the body points, same convention
    };

    // Forward direction on the ground plane for a yaw (0 -> -Z, +pi/2 -> -X).
    [[nodiscard]] glm::vec3 YawForward(float yaw);
}
