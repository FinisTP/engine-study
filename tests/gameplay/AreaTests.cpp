#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Area.hpp>
#include <GltfImporter.hpp>
#include <Interaction.hpp>
#include <PhysicsBridge.hpp>
#include <PlayerController.hpp>
#include <SceneFile.hpp>

#include <glm/gtc/constants.hpp>

#include <map>
#include <memory>

using Catch::Approx;
using namespace umbra;

namespace
{
    constexpr float step = 1.0F / 60.0F;
    const std::filesystem::path assetDir{UMBRA_TEST_ASSET_DIRECTORY};

    // Same steps as the app's SceneRuntime::Install, without rendering:
    // fresh world + fresh physics per area, player at a spawn marker.
    struct Area
    {
        std::unique_ptr<world::World> world;
        std::unique_ptr<physics::PhysicsWorld> physics;
        gameplay::PlayerController player;
        std::size_t colliders = 0;
    };

    std::map<std::string, assets::ImportResult> &Models()
    {
        static std::map<std::string, assets::ImportResult> models;
        return models;
    }

    Area Enter(const std::string &scenePath, const std::string &spawn)
    {
        scene::SceneLoadOptions options;
        options.assetRoot = assetDir;
        options.models = [](const std::string &path) -> const assets::Model * {
            auto it = Models().find(path);
            if (it == Models().end())
                it = Models().emplace(path, assets::ImportGltf(assetDir / path)).first;
            return it->second.Ok() ? &it->second.model : nullptr;
        };
        Area area{std::make_unique<world::World>(), std::make_unique<physics::PhysicsWorld>(), gameplay::PlayerController{}, 0};
        std::vector<core::Diagnostic> diagnostics;
        REQUIRE(scene::LoadScene(assetDir / scenePath, options, *area.world, diagnostics));
        area.world->UpdateTransforms();
        REQUIRE(gameplay::CreateBodies(*area.world, *area.physics) == 0);
        area.world->ForEach([&](world::Entity e) { area.colliders += area.world->GetCollider(e) ? 1 : 0; });
        const auto start = gameplay::FindSpawnPoint(*area.world, spawn);
        REQUIRE(start);
        REQUIRE(area.player.Spawn(*area.physics, start->feet, start->yaw));
        return area;
    }

    // Walks with `move` until the player's center is inside a portal; returns that portal.
    std::optional<world::Portal> WalkIntoPortal(Area &area, glm::vec2 move, float maxSeconds)
    {
        for (int i = 0; i < static_cast<int>(maxSeconds / step); ++i)
        {
            area.player.Update(*area.physics, {move, true, false}, step);
            area.physics->Step(step);
            const glm::vec3 center = area.player.GetFeet(*area.physics) + glm::vec3{0.0F, 0.9F, 0.0F};
            if (const world::Entity portal = gameplay::FindPortal(*area.world, center))
                return *area.world->GetPortal(portal);
        }
        return std::nullopt;
    }
}

TEST_CASE("Spawn points give feet and facing from a marker entity")
{
    world::World world;
    const world::Entity marker = world.Create("arrive");
    world::Transform t;
    t.position = {7.5F, 0.0F, 0.0F};
    t.rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3{0, 1, 0}); // -Z turned to -X
    world.SetLocalTransform(marker, t);
    world.UpdateTransforms();
    const auto spawn = gameplay::FindSpawnPoint(world, "arrive");
    REQUIRE(spawn);
    REQUIRE(spawn->feet.x == Approx(7.5F));
    REQUIRE(spawn->yaw == Approx(glm::half_pi<float>()));
    REQUIRE_FALSE(gameplay::FindSpawnPoint(world, "nowhere"));
}

TEST_CASE("A portal's box is its transform applied to the -1..1 cube")
{
    world::World world;
    const world::Entity gate = world.Create("gate");
    world::Transform t;
    t.position = {9.0F, 1.2F, 0.0F};
    t.scale = {0.2F, 1.2F, 1.0F};
    world.SetLocalTransform(gate, t);
    world.SetPortal(gate, {"scenes/dungeon.scene.json", "entry"});
    REQUIRE(world.Create("not_a_portal"));
    world.UpdateTransforms();
    REQUIRE(gameplay::FindPortal(world, {9.1F, 1.0F, 0.5F}) == gate);
    REQUIRE_FALSE(gameplay::FindPortal(world, {9.3F, 1.0F, 0.0F})); // past the slab
    REQUIRE_FALSE(gameplay::FindPortal(world, {9.0F, 1.0F, 1.2F})); // beside it
}

TEST_CASE("Repeated settlement <-> dungeon trips leave no stale bodies, characters or entities")
{
    Area area = Enter("scenes/settlement.scene.json", "player");
    const std::size_t settlementEntities = area.world->Count();
    std::size_t dungeonEntities = 0;

    for (int trip = 0; trip < 10; ++trip)
    {
        // Settlement: walk east into the gate (start near it to keep the test fast).
        area.player.Spawn(*area.physics, {7.0F, 0.0F, 0.0F}, -glm::half_pi<float>()); // facing +X
        const auto toDungeon = WalkIntoPortal(area, {0, 1}, 3.0F);
        REQUIRE(toDungeon);
        REQUIRE(toDungeon->scene == "scenes/dungeon.scene.json");
        area = Enter(toDungeon->scene, toDungeon->spawn); // the old area is destroyed here
        REQUIRE(area.physics->BodyCount() == area.colliders);
        REQUIRE(area.physics->CharacterCount() == 1);
        dungeonEntities = dungeonEntities ? dungeonEntities : area.world->Count();
        REQUIRE(area.world->Count() == dungeonEntities);

        // Dungeon: the entry faces -Z; walk backward (+Z) into the exit.
        const auto toSettlement = WalkIntoPortal(area, {0, -1}, 3.0F);
        REQUIRE(toSettlement);
        area = Enter(toSettlement->scene, toSettlement->spawn);
        REQUIRE(area.world->Count() == settlementEntities);
        REQUIRE(area.physics->BodyCount() == area.colliders);
        REQUIRE(area.physics->CharacterCount() == 1);
    }
}

TEST_CASE("Unloading into one long-lived physics world returns it to empty")
{
    Area area = Enter("scenes/dungeon.scene.json", "entry");
    for (int i = 0; i < 5; ++i)
    {
        gameplay::DestroyBodies(*area.world, *area.physics);
        area.player.Despawn(*area.physics);
        REQUIRE(area.physics->BodyCount() == 0);
        REQUIRE(area.physics->CharacterCount() == 0);
        REQUIRE(gameplay::CreateBodies(*area.world, *area.physics) == 0);
        REQUIRE(area.player.Spawn(*area.physics, {0, 0, 9}, 0.0F));
        REQUIRE(area.physics->BodyCount() == area.colliders);
    }
}

TEST_CASE("The dungeon chest is found from in front of it")
{
    Area area = Enter("scenes/dungeon.scene.json", "entry");
    area.player.Spawn(*area.physics, {0.0F, 0.0F, -8.4F}, 0.0F); // facing -Z, chest front at z = -9.6, top at 1.0
    area.physics->Step(step);
    const world::Entity chest = gameplay::FindInteractable(*area.world, *area.physics, area.player);
    REQUIRE(chest == area.world->Find("chest"));
}
