#include <catch2/catch_test_macros.hpp>

#include <World.hpp>

using umbra::core::DrawList;
using umbra::core::MeshHandle;
using umbra::core::TextureHandle;
using umbra::world::Entity;
using umbra::world::Renderable;
using umbra::world::Transform;
using umbra::world::World;

TEST_CASE("Draw list holds one item per renderable with its world matrix")
{
    World world;
    const MeshHandle mesh{0, 1};
    const TextureHandle texture{0, 1};
    const Entity parent = world.Create("parent");
    const Entity child = world.Create("child");
    (void)world.Create("empty"); // no renderable -> not drawn

    Transform t;
    t.position = {1, 0, 0};
    world.SetLocalTransform(parent, t);
    t.position = {0, 3, 0};
    world.SetLocalTransform(child, t);
    world.SetParent(child, parent);
    world.SetRenderable(parent, Renderable{"m.gltf", 0, glm::vec4{1}, mesh, texture});
    world.SetRenderable(child, Renderable{"m.gltf", 0, glm::vec4{0.5F}, mesh, {}});
    world.UpdateTransforms();

    DrawList list;
    world.ExtractDrawList(list);

    REQUIRE(list.items.size() == 2);
    bool foundChild = false;
    for (const auto &item : list.items)
    {
        REQUIRE(item.mesh == mesh);
        if (item.color.x == 0.5F)
        {
            foundChild = true;
            REQUIRE_FALSE(item.texture);
            REQUIRE(item.model[3].x == 1.0F);
            REQUIRE(item.model[3].y == 3.0F);
        }
    }
    REQUIRE(foundChild);
}

TEST_CASE("Renderables without a loaded mesh are not extracted")
{
    World world;
    const Entity entity = world.Create("pending");
    world.SetRenderable(entity, Renderable{"m.gltf", 0});
    world.UpdateTransforms();

    DrawList list;
    world.ExtractDrawList(list);
    REQUIRE(list.items.empty());
}

TEST_CASE("Renderable component can be read, changed and removed")
{
    World world;
    const Entity entity = world.Create("e");
    REQUIRE(world.GetRenderable(entity) == nullptr);
    REQUIRE(world.SetRenderable(entity, Renderable{"a.gltf", 2}));
    REQUIRE(world.GetRenderable(entity)->mesh == 2);
    world.GetRenderable(entity)->meshHandle = {0, 1};
    REQUIRE(world.GetRenderable(entity)->meshHandle);
    REQUIRE(world.RemoveRenderable(entity));
    REQUIRE_FALSE(world.RemoveRenderable(entity));
    REQUIRE(world.GetRenderable(entity) == nullptr);
}
