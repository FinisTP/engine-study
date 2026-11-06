#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <PhysicsBridge.hpp>

#include <glm/gtc/constants.hpp>

using Catch::Approx;
using namespace umbra;

namespace
{
    world::Entity Add(world::World &world, const std::string &id, glm::vec3 position, glm::vec3 scale, world::ColliderMotion motion)
    {
        const world::Entity entity = world.Create(id);
        world::Transform t;
        t.position = position;
        t.scale = scale;
        world.SetLocalTransform(entity, t);
        world::Collider collider;
        collider.motion = motion;
        world.SetCollider(entity, collider);
        return entity;
    }

    void Simulate(world::World &world, physics::PhysicsWorld &physics, int steps)
    {
        for (int i = 0; i < steps; ++i)
        {
            gameplay::PushKinematic(world, physics, 1.0F / 60.0F);
            physics.Step(1.0F / 60.0F);
            gameplay::PullDynamic(world, physics);
            world.UpdateTransforms();
        }
    }
}

TEST_CASE("Colliders become bodies and dynamic entities follow the simulation")
{
    world::World world;
    physics::PhysicsWorld physics;
    Add(world, "floor", {0, -1, 0}, {10, 1, 10}, world::ColliderMotion::Static); // top at y = 0
    const world::Entity crate = Add(world, "crate", {0, 4, 0}, glm::vec3{0.5F}, world::ColliderMotion::Dynamic);
    world.UpdateTransforms();

    REQUIRE(gameplay::CreateBodies(world, physics) == 0);
    REQUIRE(physics.BodyCount() == 2);
    REQUIRE(gameplay::CreateBodies(world, physics) == 0); // idempotent
    REQUIRE(physics.BodyCount() == 2);

    Simulate(world, physics, 180);
    const world::Transform &t = *world.GetLocalTransform(crate);
    REQUIRE(t.position.y == Approx(0.5F).margin(0.03));
    REQUIRE(t.scale.x == 0.5F); // scale kept

    gameplay::DestroyBodies(world, physics);
    REQUIRE(physics.BodyCount() == 0);
    REQUIRE_FALSE(world.GetCollider(crate)->body);
}

TEST_CASE("Kinematic bodies follow their entity and push dynamic bodies")
{
    world::World world;
    physics::PhysicsWorld physics;
    Add(world, "floor", {0, -1, 0}, {10, 1, 10}, world::ColliderMotion::Static);
    const world::Entity pusher = Add(world, "pusher", {-2, 0.5F, 0}, glm::vec3{0.5F}, world::ColliderMotion::Kinematic);
    const world::Entity crate = Add(world, "crate", {0, 0.5F, 0}, glm::vec3{0.5F}, world::ColliderMotion::Dynamic);
    world.UpdateTransforms();
    gameplay::CreateBodies(world, physics);

    for (int i = 0; i < 120; ++i)
    {
        world::Transform t = *world.GetLocalTransform(pusher);
        t.position.x += 3.0F / 60.0F;
        world.SetLocalTransform(pusher, t);
        world.UpdateTransforms();
        Simulate(world, physics, 1);
    }
    glm::vec3 position;
    glm::quat rotation;
    REQUIRE(physics.GetBodyTransform(world.GetCollider(pusher)->body, position, rotation));
    REQUIRE(position.x == Approx(4.0F).margin(0.1));
    REQUIRE(world.GetLocalTransform(crate)->position.x > 4.5F);
}

TEST_CASE("Colliders with unusable sizes are counted, not created")
{
    world::World world;
    physics::PhysicsWorld physics;
    const world::Entity flat = Add(world, "flat", {}, {1, 0, 1}, world::ColliderMotion::Static); // zero scale on Y
    world.UpdateTransforms();
    REQUIRE(gameplay::CreateBodies(world, physics) == 1);
    REQUIRE_FALSE(world.GetCollider(flat)->body);
}

TEST_CASE("Decompose recovers translation, rotation and scale")
{
    world::Transform t;
    t.position = {1, 2, 3};
    t.rotation = glm::angleAxis(0.7F, glm::normalize(glm::vec3{1, 1, 0}));
    t.scale = {2, 3, 4};
    world::World world;
    const world::Entity e = world.Create("e");
    world.SetLocalTransform(e, t);
    world.UpdateTransforms();

    glm::vec3 position, scale;
    glm::quat rotation;
    gameplay::Decompose(*world.GetWorldMatrix(e), position, rotation, scale);
    REQUIRE(position.z == Approx(3.0F));
    REQUIRE(scale.y == Approx(3.0F));
    REQUIRE(std::abs(glm::dot(rotation, t.rotation)) == Approx(1.0F).margin(1e-5));
}

TEST_CASE("Debug lines outline every body")
{
    physics::PhysicsWorld physics;
    physics::BodyDesc box;
    REQUIRE(physics.CreateBody(box));
    physics::BodyDesc sphere;
    sphere.shape.kind = physics::ShapeKind::Sphere;
    REQUIRE(physics.CreateBody(sphere));

    core::DrawList list;
    gameplay::AddDebugLines(physics, list);
    REQUIRE(list.lines.size() == 12 + 3 * 16);
}
