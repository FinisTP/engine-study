#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Physics.hpp>

using Catch::Approx;
using namespace umbra::physics;

namespace
{
    constexpr float step = 1.0F / 60.0F;

    // 20 x 1 x 20 floor whose top surface is at y = 0.
    BodyHandle AddFloor(PhysicsWorld &world)
    {
        BodyDesc floor;
        floor.shape.halfExtents = {10.0F, 0.5F, 10.0F};
        floor.position = {0.0F, -0.5F, 0.0F};
        return world.CreateBody(floor);
    }

    BodyHandle AddBox(PhysicsWorld &world, glm::vec3 position, glm::vec3 halfExtents, Motion motion = Motion::Static)
    {
        BodyDesc box;
        box.shape.halfExtents = halfExtents;
        box.position = position;
        box.motion = motion;
        return world.CreateBody(box);
    }

    void Run(PhysicsWorld &world, int steps)
    {
        for (int i = 0; i < steps; ++i)
            world.Step(step);
    }

    void Walk(PhysicsWorld &world, CharacterHandle character, glm::vec3 velocity, int steps, bool jump = false)
    {
        for (int i = 0; i < steps; ++i)
            world.MoveCharacter(character, velocity, jump && i == 0, step);
    }
}

TEST_CASE("A dynamic box falls and comes to rest on the floor")
{
    PhysicsWorld world;
    AddFloor(world);
    const BodyHandle box = AddBox(world, {0, 5, 0}, glm::vec3{0.5F}, Motion::Dynamic);
    REQUIRE(box);
    Run(world, 180);

    glm::vec3 position;
    glm::quat rotation;
    REQUIRE(world.GetBodyTransform(box, position, rotation));
    REQUIRE(position.y == Approx(0.5F).margin(0.03)); // Jolt allows ~2 cm penetration slop
}

TEST_CASE("Raycasts report body, point, normal and distance")
{
    PhysicsWorld world;
    const BodyHandle floor = AddFloor(world);
    world.Step(step);

    const auto hit = world.Raycast({1, 3, 2}, {0, -1, 0}, 10.0F);
    REQUIRE(hit);
    REQUIRE(hit->body == floor);
    REQUIRE(hit->distance == Approx(3.0F).margin(1e-3));
    REQUIRE(hit->normal.y == Approx(1.0F).margin(1e-3));
    REQUIRE_FALSE(world.Raycast({1, 3, 2}, {0, 1, 0}, 10.0F));
    REQUIRE_FALSE(world.Raycast({1, 3, 2}, {0, -1, 0}, 2.0F)); // too short
}

TEST_CASE("Invalid shapes are rejected and destroyed bodies go stale")
{
    PhysicsWorld world;
    REQUIRE_FALSE(AddBox(world, {}, {0.0F, 1.0F, 1.0F}));
    BodyDesc sphere;
    sphere.shape.kind = ShapeKind::Sphere;
    sphere.shape.radius = -1.0F;
    REQUIRE_FALSE(world.CreateBody(sphere));

    const BodyHandle box = AddBox(world, {}, glm::vec3{1.0F});
    REQUIRE(world.BodyCount() == 1);
    REQUIRE(world.DestroyBody(box));
    REQUIRE_FALSE(world.DestroyBody(box));
    REQUIRE(world.GetBodyShape(box) == nullptr);
    REQUIRE(world.BodyCount() == 0);
}

TEST_CASE("A character stands, walks, and is stopped by walls")
{
    PhysicsWorld world;
    AddFloor(world);
    AddBox(world, {3, 1.5F, 0}, {0.5F, 1.5F, 5.0F}); // wall from x = 2.5 to 3.5
    world.Step(step);
    const CharacterHandle player = world.CreateCharacter({{0, 1.0F, 0}});
    REQUIRE(player);

    Walk(world, player, {0, 0, 0}, 60);
    REQUIRE(world.GetCharacter(player)->grounded);
    REQUIRE(world.GetCharacter(player)->position.y == Approx(0.9F).margin(0.05)); // radius 0.3 + half height 0.6

    Walk(world, player, {2, 0, 0}, 240); // would reach x = 8 without the wall
    const glm::vec3 position = world.GetCharacter(player)->position;
    REQUIRE(position.x < 2.5F);
    REQUIRE(position.x > 2.0F);
}

TEST_CASE("A character climbs a small step but not a tall ledge")
{
    PhysicsWorld world;
    AddFloor(world);
    AddBox(world, {3, 0.1F, 0}, {1.0F, 0.1F, 5.0F});   // 0.2 m step from x = 2
    AddBox(world, {-3, 0.5F, 0}, {1.0F, 0.5F, 5.0F});  // 1.0 m ledge from x = -2
    world.Step(step);
    const CharacterHandle player = world.CreateCharacter({{0, 1.0F, 0}});

    Walk(world, player, {2, 0, 0}, 90);
    REQUIRE(world.GetCharacter(player)->position.x > 2.2F);
    REQUIRE(world.GetCharacter(player)->position.y == Approx(1.1F).margin(0.08));

    world.TeleportCharacter(player, {0, 1.0F, 0});
    Walk(world, player, {-2, 0, 0}, 120);
    REQUIRE(world.GetCharacter(player)->position.x > -2.0F);
}

TEST_CASE("Jumping leaves the ground and lands again")
{
    PhysicsWorld world;
    AddFloor(world);
    world.Step(step);
    const CharacterHandle player = world.CreateCharacter({{0, 1.0F, 0}});
    Walk(world, player, {0, 0, 0}, 30);

    float highest = 0.0F;
    world.MoveCharacter(player, {0, 0, 0}, true, step);
    for (int i = 0; i < 120; ++i)
    {
        world.MoveCharacter(player, {0, 0, 0}, false, step);
        highest = std::max(highest, world.GetCharacter(player)->position.y);
    }
    REQUIRE(highest > 1.9F); // v^2 / 2g = 25 / 19.6 = 1.27 m above 0.9
    REQUIRE(world.GetCharacter(player)->grounded);
    REQUIRE(world.GetCharacter(player)->position.y == Approx(0.9F).margin(0.05));
}

TEST_CASE("Two physics worlds can live at the same time")
{
    auto a = std::make_unique<PhysicsWorld>();
    {
        PhysicsWorld b;
        AddFloor(b);
    }
    AddFloor(*a);
    a->Step(step);
    REQUIRE(a->Raycast({0, 1, 0}, {0, -1, 0}, 5.0F));
}
