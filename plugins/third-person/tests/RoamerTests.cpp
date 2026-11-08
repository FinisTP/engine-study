#include <catch2/catch_test_macros.hpp>

#include <Roamer.hpp>

#include <glm/geometric.hpp>

using namespace umbra;

namespace
{
    constexpr float step = 1.0F / 60.0F;

    // A floor and one wall between x = -0.5 and 0.5, from z = 2 to z = 6.
    struct Room
    {
        physics::PhysicsWorld physics;
        Room()
        {
            Box({0, -0.5F, 0}, {30, 0.5F, 30});
            Box({0, 1.5F, 4}, {0.5F, 1.5F, 2});
        }
        void Box(glm::vec3 center, glm::vec3 half)
        {
            physics::BodyDesc desc{physics::Shape{physics::ShapeKind::Box, half}};
            desc.position = center;
            REQUIRE(physics.CreateBody(desc));
        }
        void Run(gameplay::Roamer &r, glm::vec3 player, float seconds, std::mt19937 &rng)
        {
            for (int i = 0; i < static_cast<int>(seconds / step); ++i)
            {
                r.Update(physics, player, step, rng);
                physics.Step(step);
            }
        }
    };
}

TEST_CASE("Sight is blocked by walls and limited by range")
{
    Room room;
    REQUIRE(gameplay::CanSee(room.physics, {-3, 0.8F, 4}, {-3, 0.8F, 0}, 8.0F));      // open floor
    REQUIRE_FALSE(gameplay::CanSee(room.physics, {-3, 0.8F, 4}, {3, 0.8F, 4}, 8.0F)); // through the wall
    REQUIRE_FALSE(gameplay::CanSee(room.physics, {-3, 0.8F, -4}, {-3, 0.8F, 6}, 8.0F)); // too far
}

TEST_CASE("A chaser runs at a visible player; a wanderer stays near home")
{
    Room room;
    std::mt19937 rng(7);
    gameplay::Roamer chaser("wolf", {-3, 0, -2}, 1.0F, true);
    REQUIRE(chaser.Spawn(room.physics, {-3, 0, -2}));
    room.Run(chaser, {-3, 0, 3}, 1.5F, rng);
    REQUIRE(chaser.IsChasing());
    REQUIRE(glm::length(chaser.GetFeet(room.physics) - glm::vec3{-3, 0, 3}) < 2.5F);

    gameplay::Roamer wanderer("wolf", {5, 0, -5}, 2.0F, false);
    REQUIRE(wanderer.Spawn(room.physics, {5, 0, -5}));
    for (int i = 0; i < 20; ++i)
    {
        room.Run(wanderer, {-3, 0, 3}, 0.5F, rng);
        const glm::vec3 feet = wanderer.GetFeet(room.physics);
        REQUIRE(glm::length(glm::vec2{feet.x - 5, feet.z + 5}) < 2.5F);
    }
    REQUIRE_FALSE(wanderer.IsChasing());

    // Behind the wall the player is unseen, so a chaser that never saw it keeps wandering.
    gameplay::Roamer blind("wolf", {3, 0, 4}, 0.5F, true);
    REQUIRE(blind.Spawn(room.physics, {3, 0, 4}));
    room.Run(blind, {-3, 0, 4}, 1.0F, rng);
    REQUIRE_FALSE(blind.IsChasing());

    chaser.Despawn(room.physics);
    wanderer.Despawn(room.physics);
    blind.Despawn(room.physics);
    REQUIRE(room.physics.CharacterCount() == 0);
}
