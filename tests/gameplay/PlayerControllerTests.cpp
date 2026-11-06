#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <PlayerController.hpp>

#include <glm/gtc/constants.hpp>

using Catch::Approx;
using namespace umbra;

namespace
{
    constexpr float step = 1.0F / 60.0F;

    physics::BodyHandle AddBox(physics::PhysicsWorld &physics, glm::vec3 position, glm::vec3 halfExtents)
    {
        physics::BodyDesc desc;
        desc.position = position;
        desc.shape.halfExtents = halfExtents;
        return physics.CreateBody(desc);
    }

    // Flat 40 x 40 floor with its top at y = 0.
    void AddFloor(physics::PhysicsWorld &physics) { (void)AddBox(physics, {0, -0.5F, 0}, {20, 0.5F, 20}); }

    void Run(gameplay::PlayerController &player, physics::PhysicsWorld &physics, gameplay::PlayerInput input, int steps)
    {
        for (int i = 0; i < steps; ++i)
        {
            player.Update(physics, input, step);
            input.jump = false;
        }
    }
}

TEST_CASE("Forward input moves away from the camera, at walking or sprinting speed")
{
    physics::PhysicsWorld physics;
    AddFloor(physics);
    physics.Step(step);
    gameplay::PlayerController player;
    REQUIRE(player.Spawn(physics, {0, 0, 0}, 0.0F));
    Run(player, physics, {}, 30); // settle on the ground
    REQUIRE(player.IsGrounded(physics));
    REQUIRE(player.GetFeet(physics).y == Approx(0.0F).margin(0.05));

    Run(player, physics, {{0, 1}}, 60); // 1 s forward, camera yaw 0 -> -Z
    const glm::vec3 walked = player.GetFeet(physics);
    REQUIRE(walked.z == Approx(-3.5F).margin(0.2));
    REQUIRE(walked.x == Approx(0.0F).margin(0.05));

    Run(player, physics, {{0, 1}, true}, 60);
    REQUIRE(player.GetFeet(physics).z - walked.z == Approx(-6.5F).margin(0.3));
}

TEST_CASE("Movement follows the camera yaw and diagonals are not faster")
{
    physics::PhysicsWorld physics;
    AddFloor(physics);
    physics.Step(step);
    gameplay::PlayerController player;
    player.Spawn(physics, {0, 0, 0}, 0.0F);
    Run(player, physics, {}, 30);

    player.Look(glm::half_pi<float>(), 0.0F); // camera now faces -X
    Run(player, physics, {{0, 1}}, 60);
    REQUIRE(player.GetFeet(physics).x == Approx(-3.5F).margin(0.2));

    const glm::vec3 before = player.GetFeet(physics);
    Run(player, physics, {{1, 1}}, 60); // diagonal
    REQUIRE(glm::length(player.GetFeet(physics) - before) == Approx(3.5F).margin(0.2));
}

TEST_CASE("The body turns toward the movement direction")
{
    physics::PhysicsWorld physics;
    AddFloor(physics);
    physics.Step(step);
    gameplay::PlayerController player;
    player.Spawn(physics, {0, 0, 0}, 0.0F);
    Run(player, physics, {{-1, 0}}, 60); // strafe left (-X)
    REQUIRE(player.GetFacingYaw() == Approx(glm::half_pi<float>()).margin(0.05));
}

TEST_CASE("The camera orbits behind and is pulled in front of walls")
{
    physics::PhysicsWorld physics;
    AddFloor(physics);
    physics.Step(step);
    gameplay::PlayerController player;
    player.Spawn(physics, {0, 0, 0}, 0.0F);
    Run(player, physics, {}, 30);

    const core::Camera open = player.GetCamera(physics);
    REQUIRE(open.position.z > 3.0F); // behind = +Z when looking toward -Z
    REQUIRE(open.position.y > open.target.y); // pitched down, so above the pivot
    REQUIRE(glm::length(open.position - open.target) == Approx(4.5F).margin(0.01));

    (void)AddBox(physics, {0, 2, 2}, {5, 3, 0.2F}); // wall 2 m behind the player
    physics.Step(step);
    const core::Camera blocked = player.GetCamera(physics);
    REQUIRE(blocked.position.z < 1.9F);
    REQUIRE(glm::length(blocked.position - blocked.target) >= 0.3F);
}

TEST_CASE("Pitch is clamped")
{
    gameplay::PlayerController player;
    player.Look(0.0F, -10.0F);
    REQUIRE(player.GetCameraPitch() == Approx(player.GetSettings().minPitch));
    player.Look(0.0F, 10.0F);
    REQUIRE(player.GetCameraPitch() == Approx(player.GetSettings().maxPitch));
}

TEST_CASE("Interaction query finds what the player faces within reach")
{
    physics::PhysicsWorld physics;
    AddFloor(physics);
    const physics::BodyHandle crate = AddBox(physics, {0, 0.5F, -1.5F}, glm::vec3{0.5F});
    physics.Step(step);
    gameplay::PlayerController player;
    player.Spawn(physics, {0, 0, 0}, 0.0F);
    Run(player, physics, {}, 10);

    const auto hit = player.InteractionQuery(physics);
    REQUIRE(hit);
    REQUIRE(hit->body == crate);

    player.Spawn(physics, {0, 0, 0}, glm::pi<float>()); // facing +Z: nothing there
    Run(player, physics, {}, 10);
    REQUIRE_FALSE(player.InteractionQuery(physics));
}

TEST_CASE("Despawn removes the character and stops updates")
{
    physics::PhysicsWorld physics;
    gameplay::PlayerController player;
    player.Spawn(physics, {0, 0, 0}, 0.0F);
    REQUIRE(physics.CharacterCount() == 1);
    player.Spawn(physics, {1, 0, 0}, 0.0F); // respawn replaces, never duplicates
    REQUIRE(physics.CharacterCount() == 1);
    player.Despawn(physics);
    REQUIRE(physics.CharacterCount() == 0);
    REQUIRE_FALSE(player.IsSpawned());
    player.Update(physics, {{0, 1}}, step); // harmless
    REQUIRE_FALSE(player.InteractionQuery(physics));
}
