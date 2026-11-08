#include <ModelTools.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace umbra;

TEST_CASE("A model's size is measured and a collider fits it (T73)")
{
    // The reference wolf: real triangles and bounds from the game's own importer.
    authoring::ModelInfo info;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(authoring::InspectModel(std::filesystem::path(UMBRA_TEST_ASSET_DIRECTORY) / "models/wolf.glb", info, diagnostics));
    CHECK(info.triangles > 100);
    CHECK(info.max.y > info.min.y);

    // A 1 m cube at scale 2: a static box with 1 m half sizes; each axis on its own.
    info.min = {-0.5F, -0.5F, -0.5F};
    info.max = {0.5F, 1.5F, 0.5F};
    const auto collider = authoring::FittedCollider(info, {2.0F, 2.0F, -2.0F});
    CHECK(collider["shape"] == "box");
    CHECK(collider["motion"] == "static");
    CHECK(collider["halfExtents"] == nlohmann::ordered_json::array({1.0F, 2.0F, 1.0F}));

    // A file the importer refuses: an error, no numbers.
    CHECK_FALSE(authoring::InspectModel(std::filesystem::path(UMBRA_TEST_ASSET_DIRECTORY) / "rpg/reference.rpg.json", info, diagnostics));
    CHECK(core::HasErrors(diagnostics));
}
