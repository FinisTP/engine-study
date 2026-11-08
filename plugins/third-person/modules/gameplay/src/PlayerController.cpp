#include <PlayerController.hpp>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace umbra::gameplay
{
    glm::vec3 YawForward(float yaw) { return {-std::sin(yaw), 0.0F, -std::cos(yaw)}; }

    namespace
    {
        // Shortest signed angle from a to b, in (-pi, pi].
        float AngleDifference(float a, float b)
        {
            return std::remainder(b - a, glm::two_pi<float>());
        }
    }

    bool PlayerController::Spawn(physics::PhysicsWorld &physics, const glm::vec3 &feet, float yaw)
    {
        Despawn(physics);
        physics::CharacterDesc desc;
        desc.radius = settings_.radius;
        desc.halfHeight = settings_.halfHeight;
        desc.position = feet + glm::vec3{0.0F, settings_.radius + settings_.halfHeight, 0.0F};
        character_ = physics.CreateCharacter(desc);
        cameraYaw_ = facingYaw_ = yaw;
        return IsSpawned();
    }

    void PlayerController::Despawn(physics::PhysicsWorld &physics)
    {
        physics.DestroyCharacter(character_);
        character_ = {};
    }

    void PlayerController::Look(float yawDelta, float pitchDelta)
    {
        cameraYaw_ = std::remainder(cameraYaw_ + yawDelta, glm::two_pi<float>());
        cameraPitch_ = std::clamp(cameraPitch_ + pitchDelta, settings_.minPitch, settings_.maxPitch);
    }

    void PlayerController::Update(physics::PhysicsWorld &physics, const PlayerInput &input, float deltaSeconds)
    {
        if (!IsSpawned())
            return;

        // 1. Camera-relative direction on the ground; diagonals are not faster.
        const glm::vec3 forward = YawForward(cameraYaw_);
        const glm::vec3 right{-forward.z, 0.0F, forward.x};
        glm::vec3 direction = right * input.move.x + forward * input.move.y;
        const float length = glm::length(direction);
        if (length > 1.0F)
            direction /= length;

        // 2. Turn the body smoothly toward where it's going.
        if (length > 0.01F)
        {
            const float target = std::atan2(-direction.x, -direction.z);
            const float turn = std::clamp(settings_.turnSpeed * deltaSeconds, 0.0F, 1.0F);
            facingYaw_ = std::remainder(facingYaw_ + AngleDifference(facingYaw_, target) * turn, glm::two_pi<float>());
        }

        const float speed = input.sprint ? settings_.sprintSpeed : settings_.walkSpeed;
        physics.MoveCharacter(character_, direction * speed, input.jump, deltaSeconds);
    }

    glm::vec3 PlayerController::Center(const physics::PhysicsWorld &physics) const
    {
        const auto state = physics.GetCharacter(character_);
        return state ? state->position : glm::vec3{0.0F};
    }

    glm::vec3 PlayerController::GetFeet(const physics::PhysicsWorld &physics) const
    {
        return Center(physics) - glm::vec3{0.0F, settings_.radius + settings_.halfHeight, 0.0F};
    }

    glm::vec3 PlayerController::GetVelocity(const physics::PhysicsWorld &physics) const
    {
        const auto state = physics.GetCharacter(character_);
        return state ? state->velocity : glm::vec3{0.0F};
    }

    bool PlayerController::IsGrounded(const physics::PhysicsWorld &physics) const
    {
        const auto state = physics.GetCharacter(character_);
        return state && state->grounded;
    }

    core::Camera PlayerController::GetCamera(const physics::PhysicsWorld &physics) const
    {
        // 1. Orbit: start at a pivot above the player and back away along the view direction.
        const glm::vec3 pivot = Center(physics) + glm::vec3{0.0F, settings_.cameraHeight, 0.0F};
        const glm::vec3 view = std::cos(cameraPitch_) * YawForward(cameraYaw_) + glm::vec3{0.0F, std::sin(cameraPitch_), 0.0F};

        // 2. If a wall is between pivot and camera, stop just in front of it.
        float distance = settings_.cameraDistance;
        if (const auto hit = physics.Raycast(pivot, -view, distance))
            distance = std::max(hit->distance - 0.2F, 0.3F);

        core::Camera camera;
        camera.position = pivot - view * distance;
        camera.target = pivot;
        return camera;
    }

    std::optional<physics::RayHit> PlayerController::InteractionQuery(const physics::PhysicsWorld &physics, bool low) const
    {
        if (!IsSpawned())
            return std::nullopt;
        // Waist height (capsule center) or knee height (0.3 m above the feet), from the capsule's front edge.
        const glm::vec3 forward = YawForward(facingYaw_);
        glm::vec3 origin = Center(physics) + forward * settings_.radius;
        if (low)
            origin.y = GetFeet(physics).y + 0.3F;
        return physics.Raycast(origin, forward, settings_.interactDistance);
    }

    void PlayerController::AddDebugLines(const physics::PhysicsWorld &physics, core::DrawList &drawList) const
    {
        if (!IsSpawned())
            return;
        // Capsule outline (yellow) plus a short facing arrow.
        const glm::vec4 color{1.0F, 0.9F, 0.2F, 1.0F};
        const glm::vec3 center = Center(physics);
        const glm::vec3 up{0.0F, settings_.halfHeight, 0.0F};
        constexpr int segments = 16;
        for (const glm::vec3 &ring : {center + up, center - up})
        {
            for (int i = 0; i < segments; ++i)
            {
                const float a = glm::two_pi<float>() * static_cast<float>(i) / segments;
                const float b = glm::two_pi<float>() * static_cast<float>(i + 1) / segments;
                drawList.lines.push_back({ring + settings_.radius * glm::vec3{std::cos(a), 0, std::sin(a)},
                                          ring + settings_.radius * glm::vec3{std::cos(b), 0, std::sin(b)}, color});
            }
        }
        for (const glm::vec3 side : {glm::vec3{1, 0, 0}, glm::vec3{-1, 0, 0}, glm::vec3{0, 0, 1}, glm::vec3{0, 0, -1}})
            drawList.lines.push_back({center + up + side * settings_.radius, center - up + side * settings_.radius, color});
        drawList.lines.push_back({center, center + YawForward(facingYaw_) * (settings_.radius + 0.4F), color});
    }
}
