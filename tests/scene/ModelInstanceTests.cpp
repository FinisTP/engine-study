#include <catch2/catch_test_macros.hpp>

#include <ModelInstance.hpp>

using namespace umbra;

namespace
{
    // Root -> Arm -> Hand, where Hand has two meshes.
    assets::Model ArmModel()
    {
        assets::Model model;
        model.nodes.push_back({"Root", -1});
        model.nodes.push_back({"Arm", 0});
        model.nodes.back().position = {1, 0, 0};
        model.nodes.back().meshes = {0};
        model.nodes.push_back({"Hand", 1});
        model.nodes.back().position = {0, 2, 0};
        model.nodes.back().meshes = {1, 2};
        model.meshes.resize(3);
        return model;
    }
}

TEST_CASE("Model nodes become a matching entity hierarchy")
{
    world::World world;
    const auto entities = scene::InstantiateModel(world, ArmModel(), "models/arm.gltf", "arm");

    REQUIRE(entities.size() == 3);
    REQUIRE(*world.GetId(entities[0]) == "arm/Root");
    REQUIRE(world.GetParent(entities[1]) == entities[0]);
    REQUIRE(world.GetParent(entities[2]) == entities[1]);
    REQUIRE(world.GetRenderable(entities[0]) == nullptr);
    REQUIRE(world.GetRenderable(entities[1])->model == "models/arm.gltf");
    REQUIRE(world.GetRenderable(entities[1])->mesh == 0);

    // The second mesh of Hand lives on a child entity.
    const world::Entity extra = world.Find("arm/Hand#1");
    REQUIRE(extra);
    REQUIRE(world.GetParent(extra) == entities[2]);
    REQUIRE(world.GetRenderable(extra)->mesh == 2);
    REQUIRE(world.GetRenderable(entities[2])->mesh == 1);

    world.UpdateTransforms();
    REQUIRE((*world.GetWorldMatrix(entities[2]))[3].x == 1.0F);
    REQUIRE((*world.GetWorldMatrix(entities[2]))[3].y == 2.0F);
}

TEST_CASE("Instantiating twice gives unique ids and can attach under a parent")
{
    world::World world;
    const world::Entity holder = world.Create("holder");
    scene::InstantiateModel(world, ArmModel(), "a.gltf", "arm");
    const auto second = scene::InstantiateModel(world, ArmModel(), "a.gltf", "arm", holder);

    REQUIRE(*world.GetId(second[0]) == "arm/Root#1");
    REQUIRE(world.GetParent(second[0]) == holder);
}

TEST_CASE("Binding fills handles and counts what stays unresolved")
{
    world::World world;
    scene::InstantiateModel(world, ArmModel(), "a.gltf", "arm");
    const std::size_t unresolved = scene::BindRenderables(world, [](const std::string &, std::uint32_t mesh) {
        return std::pair{mesh == 2 ? core::MeshHandle{} : core::MeshHandle{mesh, 1}, core::TextureHandle{}};
    });

    REQUIRE(unresolved == 1);
    REQUIRE(world.GetRenderable(world.Find("arm/Arm"))->meshHandle);
    REQUIRE_FALSE(world.GetRenderable(world.Find("arm/Hand#1"))->meshHandle);
}
