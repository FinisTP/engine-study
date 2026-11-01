#include <catch2/catch_test_macros.hpp>

#include <World.hpp>

using umbra::world::Entity;
using umbra::world::World;

TEST_CASE("World creates, finds and destroys entities")
{
    World world;
    const Entity crate = world.Create("crate_a", "Crate A");

    REQUIRE(world.IsAlive(crate));
    REQUIRE(world.Find("crate_a") == crate);
    REQUIRE(*world.GetId(crate) == "crate_a");
    REQUIRE(*world.GetName(crate) == "Crate A");
    REQUIRE(world.Count() == 1);

    REQUIRE(world.Destroy(crate));
    REQUIRE_FALSE(world.IsAlive(crate));
    REQUIRE_FALSE(world.Find("crate_a"));
    REQUIRE(world.GetId(crate) == nullptr);
    REQUIRE_FALSE(world.Destroy(crate));
    REQUIRE(world.Count() == 0);
}

TEST_CASE("World rejects empty and duplicate durable ids")
{
    World world;
    REQUIRE_FALSE(world.Create(""));
    REQUIRE(world.Create("npc"));
    REQUIRE_FALSE(world.Create("npc"));
    REQUIRE(world.Count() == 1);
}

TEST_CASE("Slot reuse does not revive a destroyed entity handle")
{
    World world;
    const Entity old = world.Create("a");
    world.Destroy(old);
    const Entity reused = world.Create("b");

    REQUIRE(reused.slot.index == old.slot.index);
    REQUIRE_FALSE(world.IsAlive(old));
    REQUIRE(world.IsAlive(reused));
}

TEST_CASE("A durable id can be reused after its entity is destroyed")
{
    World world;
    const Entity first = world.Create("door");
    world.Destroy(first);
    const Entity second = world.Create("door");

    REQUIRE(second);
    REQUIRE(world.Find("door") == second);
    REQUIRE_FALSE(world.IsAlive(first));
}

TEST_CASE("Handles from another world are rejected")
{
    World a;
    World b;
    const Entity fromA = a.Create("x");
    const Entity fromB = b.Create("x");

    REQUIRE(fromA.slot == fromB.slot);
    REQUIRE(a.GetWorldId() != b.GetWorldId());
    REQUIRE_FALSE(b.IsAlive(fromA));
    REQUIRE_FALSE(b.Destroy(fromA));
    REQUIRE(b.IsAlive(fromB));
}

TEST_CASE("Clear removes everything and old handles stay stale")
{
    World world;
    const Entity a = world.Create("a");
    (void)world.Create("b");
    world.Clear();

    REQUIRE(world.Count() == 0);
    REQUIRE_FALSE(world.IsAlive(a));
    REQUIRE_FALSE(world.Find("a"));

    const Entity again = world.Create("a");
    REQUIRE(world.IsAlive(again));
    REQUIRE_FALSE(world.IsAlive(a));
}
