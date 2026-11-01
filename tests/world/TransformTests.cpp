#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <World.hpp>

#include <glm/gtc/constants.hpp>

using Catch::Approx;
using umbra::world::Entity;
using umbra::world::Transform;
using umbra::world::World;

namespace
{
    glm::vec3 WorldPosition(const World &world, Entity entity)
    {
        return glm::vec3((*world.GetWorldMatrix(entity))[3]);
    }

    void RequireNear(glm::vec3 actual, glm::vec3 expected)
    {
        REQUIRE(actual.x == Approx(expected.x).margin(1e-5));
        REQUIRE(actual.y == Approx(expected.y).margin(1e-5));
        REQUIRE(actual.z == Approx(expected.z).margin(1e-5));
    }

    Transform At(glm::vec3 position)
    {
        Transform t;
        t.position = position;
        return t;
    }
}

TEST_CASE("Child world position composes with parent translation")
{
    World world;
    const Entity parent = world.Create("parent");
    const Entity child = world.Create("child");
    world.SetLocalTransform(parent, At({1, 0, 0}));
    world.SetLocalTransform(child, At({0, 2, 0}));
    REQUIRE(world.SetParent(child, parent));
    world.UpdateTransforms();

    RequireNear(WorldPosition(world, child), {1, 2, 0});
}

TEST_CASE("Parent rotation and scale apply to the child")
{
    World world;
    const Entity parent = world.Create("parent");
    const Entity child = world.Create("child");
    Transform parentTransform = At({5, 0, 0});
    parentTransform.rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3{0, 1, 0});
    parentTransform.scale = glm::vec3{2.0F};
    world.SetLocalTransform(parent, parentTransform);
    world.SetLocalTransform(child, At({1, 0, 0}));
    world.SetParent(child, parent);
    world.UpdateTransforms();

    // +X rotated 90 degrees about +Y becomes -Z, scaled by 2, then moved by the parent.
    RequireNear(WorldPosition(world, child), {5, 0, -2});
}

TEST_CASE("Moving a parent refreshes its descendants")
{
    World world;
    const Entity a = world.Create("a");
    const Entity b = world.Create("b");
    const Entity c = world.Create("c");
    world.SetParent(b, a);
    world.SetParent(c, b);
    world.SetLocalTransform(c, At({0, 0, 1}));
    world.UpdateTransforms();
    RequireNear(WorldPosition(world, c), {0, 0, 1});

    world.SetLocalTransform(a, At({10, 0, 0}));
    world.UpdateTransforms();
    RequireNear(WorldPosition(world, c), {10, 0, 1});
}

TEST_CASE("Cycles and self-parenting are rejected")
{
    World world;
    const Entity a = world.Create("a");
    const Entity b = world.Create("b");
    const Entity c = world.Create("c");
    world.SetParent(b, a);
    world.SetParent(c, b);

    REQUIRE_FALSE(world.SetParent(a, c));
    REQUIRE_FALSE(world.SetParent(a, a));
    REQUIRE_FALSE(world.GetParent(a));
    REQUIRE(world.GetParent(c) == b);
}

TEST_CASE("Destroying a parent destroys its subtree")
{
    World world;
    const Entity root = world.Create("root");
    const Entity child = world.Create("child");
    const Entity grandchild = world.Create("grandchild");
    const Entity other = world.Create("other");
    world.SetParent(child, root);
    world.SetParent(grandchild, child);

    REQUIRE(world.Destroy(root));
    REQUIRE_FALSE(world.IsAlive(child));
    REQUIRE_FALSE(world.IsAlive(grandchild));
    REQUIRE(world.IsAlive(other));
    REQUIRE(world.Count() == 1);
    world.UpdateTransforms();
}

TEST_CASE("Destroying a child unlinks it from the parent")
{
    World world;
    const Entity parent = world.Create("parent");
    const Entity child = world.Create("child");
    world.SetParent(child, parent);
    world.Destroy(child);

    REQUIRE(world.GetChildren(parent)->empty());
}

TEST_CASE("Reparenting to root keeps the local transform")
{
    World world;
    const Entity parent = world.Create("parent");
    const Entity child = world.Create("child");
    world.SetLocalTransform(parent, At({3, 0, 0}));
    world.SetLocalTransform(child, At({1, 0, 0}));
    world.SetParent(child, parent);
    world.UpdateTransforms();
    RequireNear(WorldPosition(world, child), {4, 0, 0});

    REQUIRE(world.SetParent(child, {}));
    world.UpdateTransforms();
    RequireNear(WorldPosition(world, child), {1, 0, 0});
    REQUIRE(world.GetChildren(parent)->empty());
}

TEST_CASE("Dead or foreign parents are rejected")
{
    World world;
    World other;
    const Entity child = world.Create("child");
    const Entity dead = world.Create("dead");
    world.Destroy(dead);
    const Entity foreign = other.Create("foreign");

    REQUIRE_FALSE(world.SetParent(child, dead));
    REQUIRE_FALSE(world.SetParent(child, foreign));
    REQUIRE_FALSE(world.SetLocalTransform(dead, {}));
}
