#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <GltfImporter.hpp>
#include <Interaction.hpp>
#include <PhysicsBridge.hpp>
#include <PlayerController.hpp>
#include <SceneFile.hpp>

#include <glm/gtc/constants.hpp>

#include <map>

using Catch::Approx;
using namespace umbra;

namespace
{
    constexpr float step = 1.0F / 60.0F;

    // The real settlement test scene with bodies for every collider.
    struct Settlement
    {
        world::World world;
        physics::PhysicsWorld physics;
        gameplay::PlayerController player;
        std::map<std::string, assets::ImportResult> models;

        Settlement()
        {
            const std::filesystem::path assetDir{UMBRA_TEST_ASSET_DIRECTORY};
            scene::SceneLoadOptions options;
            options.assetRoot = assetDir;
            options.models = [&](const std::string &path) -> const assets::Model * {
                auto it = models.emplace(path, assets::ImportGltf(assetDir / path)).first;
                return it->second.Ok() ? &it->second.model : nullptr;
            };
            std::vector<core::Diagnostic> diagnostics;
            REQUIRE(scene::LoadScene(assetDir / "scenes" / "settlement.scene.json", options, world, diagnostics));
            world.UpdateTransforms();
            REQUIRE(gameplay::CreateBodies(world, physics) == 0);
            physics.Step(step);
        }

        void Walk(glm::vec2 move, float seconds, bool jumpFirst = false)
        {
            const int steps = static_cast<int>(seconds / step);
            for (int i = 0; i < steps; ++i)
            {
                player.Update(physics, {move, false, jumpFirst && i == 0}, step);
                physics.Step(step);
            }
        }

        // Walks until `arrived(feet)` or the time runs out.
        template <typename Arrived>
        void WalkUntil(glm::vec2 move, Arrived arrived, float maxSeconds)
        {
            for (int i = 0; i < static_cast<int>(maxSeconds / step) && !arrived(player.GetFeet(physics)); ++i)
            {
                player.Update(physics, {move, false, false}, step);
                physics.Step(step);
            }
        }
    };
}

TEST_CASE("The low wall blocks walking and can be jumped over")
{
    Settlement s;
    REQUIRE(s.player.Spawn(s.physics, {0, 0, 0}, 0.0F)); // the scene's spawn, facing -Z
    s.Walk({0, 1}, 3.0F);
    const float blockedZ = s.player.GetFeet(s.physics).z;
    REQUIRE(blockedZ > -5.85F);  // wall front face at z = -5.85
    REQUIRE(blockedZ < -5.0F);

    s.player.Spawn(s.physics, {0, 0, -4}, 0.0F);
    s.Walk({0, 1}, 0.3F);
    s.Walk({0, 1}, 1.5F, true); // run-up, then jump while moving
    REQUIRE(s.player.GetFeet(s.physics).z < -6.3F);
}

TEST_CASE("The stairs lead up to the platform")
{
    Settlement s;
    s.player.Spawn(s.physics, {2, 0, -4}, -glm::half_pi<float>()); // facing +X
    s.WalkUntil({0, 1}, [](glm::vec3 feet) { return feet.x > 5.6F; }, 4.0F); // platform spans x 5.2 .. 7.0
    s.Walk({0, 0}, 0.3F);
    const glm::vec3 feet = s.player.GetFeet(s.physics);
    REQUIRE(feet.x > 5.3F);
    REQUIRE(feet.y == Approx(1.0F).margin(0.1));
    REQUIRE(s.player.IsGrounded(s.physics));
}

TEST_CASE("The ramp leads up to its platform")
{
    Settlement s;
    s.player.Spawn(s.physics, {-1, 0, -3}, glm::half_pi<float>()); // facing -X
    s.WalkUntil({0, 1}, [](glm::vec3 feet) { return feet.x < -6.6F; }, 5.0F); // platform spans x -8.2 .. -5.8
    s.Walk({0, 0}, 0.3F);
    const glm::vec3 feet = s.player.GetFeet(s.physics);
    REQUIRE(feet.x < -6.0F);
    REQUIRE(feet.y == Approx(1.1F).margin(0.1));
}

TEST_CASE("Facing the signpost within reach finds it; facing away or too far does not")
{
    Settlement s;
    // The sign stands at z = 3 (front face z = 2.92). Walk toward it from the spawn.
    REQUIRE(s.player.Spawn(s.physics, {0, 0, 0}, 0.0F));
    REQUIRE_FALSE(gameplay::FindInteractable(s.world, s.physics, s.player)); // facing -Z, away from it

    s.WalkUntil({0, -1}, [](glm::vec3 feet) { return feet.z > 1.0F; }, 3.0F); // walk backward = +Z
    s.Walk({0, 0}, 0.5F); // stop and finish turning
    const world::Entity found = gameplay::FindInteractable(s.world, s.physics, s.player);
    REQUIRE(found == s.world.Find("signpost"));
    REQUIRE(s.world.GetInteractable(found)->prompt == "Read sign");

    // Out of reach: 2 m interact distance from the capsule front.
    REQUIRE(s.player.Spawn(s.physics, {0, 0, -1}, glm::pi<float>()));
    REQUIRE_FALSE(gameplay::FindInteractable(s.world, s.physics, s.player));
}
