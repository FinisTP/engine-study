#include <AnimGraph.hpp>
#include <GltfImporter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>

using namespace umbra;

namespace
{
    const std::filesystem::path assetDir = UMBRA_TEST_ASSET_DIRECTORY;

    bool Has(const std::vector<core::Diagnostic> &diagnostics, const std::string &code)
    {
        for (const core::Diagnostic &d : diagnostics)
            if (d.code == code)
                return true;
        return false;
    }

    std::vector<core::Diagnostic> Parse(const std::string &text)
    {
        core::AnimGraph graph;
        std::vector<core::Diagnostic> diagnostics;
        scene::ParseAnimGraph(text, "test.anim.json", graph, diagnostics);
        return diagnostics;
    }

    const char *header = R"("format": "umbra.animgraph", "version": 1, )";
}

TEST_CASE("The reference animation graphs load; mistakes are named by field (T87)", "[animation]")
{
    for (const char *file : {"animations/humanoid.anim.json", "animations/wolf.anim.json"})
    {
        std::vector<core::Diagnostic> diagnostics;
        const auto graph = scene::LoadAnimGraph(assetDir / file, file, diagnostics);
        INFO(file);
        REQUIRE(graph);
        CHECK(graph->states.size() == 6);
        CHECK(graph->states.front().flag == "dead"); // the important states come first
        CHECK(graph->states.back().name == "idle");
    }
    CHECK(Has(Parse(R"({"format": "umbra.scene"})"), "animgraph.format"));
    CHECK(Has(Parse(std::string("{") + header + R"("states": []})"), "animgraph.states"));
    CHECK(Has(Parse(std::string("{") + header + R"("fade": 5, "states": [{"name": "a", "clip": "a"}]})"), "animgraph.fade"));
    CHECK(Has(Parse(std::string("{") + header + R"("states": [{"name": "a"}]})"), "animgraph.state"));
    CHECK(Has(Parse(std::string("{") + header + R"("states": [{"name": "a", "clip": "a"}, {"name": "a", "clip": "b"}]})"), "animgraph.state"));
    CHECK(Has(Parse(std::string("{") + header + R"("states": [{"name": "a", "clip": "a", "when": {}}]})"), "animgraph.when"));
    CHECK(Has(Parse(std::string("{") + header + R"("states": [{"name": "a", "clip": "a", "on": "x", "when": {"flag": "f"}}]})"), "animgraph.when"));
    CHECK(Has(Parse(std::string("{") + header + R"("states": [{"name": "a", "clip": "a", "rate": {"param": "speed"}}]})"), "animgraph.rate"));
    CHECK(Has(Parse(std::string("{") + header + R"("states": [{"name": "a", "clip": "a", "events": [{"at": 1}]}]})"), "animgraph.event"));
    std::vector<core::Diagnostic> missing;
    CHECK_FALSE(scene::LoadAnimGraph(assetDir / "animations/none.anim.json", "none", missing));
    CHECK(Has(missing, "animgraph.read"));
}

TEST_CASE("Rigged entities get animators; problems are reported, never fatal (T87)", "[animation]")
{
    // The real packed hero (rigged) and elder (not rigged).
    const auto load = [](const char *file) {
        auto imported = assets::ImportGltf(assetDir / file);
        REQUIRE(imported.Ok());
        return std::make_shared<const assets::Model>(std::move(imported.model));
    };
    const auto hero = load("models/hero.glb"), elder = load("models/elder.glb");
    const scene::ModelFinder models = [&](const std::string &path) -> std::shared_ptr<const assets::Model> {
        return path == "models/hero.glb" ? hero : path == "models/elder.glb" ? elder : nullptr;
    };
    std::vector<core::Diagnostic> ignored;
    const auto humanoid = scene::LoadAnimGraph(assetDir / "animations/humanoid.anim.json", "humanoid", ignored);
    const scene::GraphFinder graphs = [&](const std::string &path) -> std::shared_ptr<const core::AnimGraph> {
        return path == "animations/humanoid.anim.json" ? humanoid : nullptr;
    };

    world::World world;
    const auto add = [&](const char *id, const char *model, const char *graph) {
        const world::Entity e = world.Create(id);
        world::Renderable look;
        look.model = model;
        look.meshHandle = {0, 1}; // any valid handle: the draw list only needs one
        world.SetRenderable(e, look);
        if (*graph != '\0')
            world.SetAnimationGraph(e, graph);
        return e;
    };
    const auto player = add("player", "models/hero.glb", "animations/humanoid.anim.json");
    const auto statue = add("statue", "models/hero.glb", "");                       // rigged, no component
    const auto elderE = add("elder", "models/elder.glb", "animations/humanoid.anim.json"); // not rigged
    const auto ghost = add("ghost", "models/hero.glb", "animations/missing.anim.json");
    std::vector<core::Diagnostic> diagnostics;
    scene::BindAnimators(world, models, graphs, diagnostics);

    // 1. The player plays its graph: idle, then walk when it moves.
    core::Animator *animator = world.GetAnimator(player);
    REQUIRE(animator != nullptr);
    CHECK_FALSE(animator->Missing());
    world.UpdateAnimation(0.1F);
    CHECK(animator->State() == "idle");
    animator->SetNumber("speed", 1.5F);
    world.UpdateAnimation(0.1F);
    CHECK(animator->State() == "walk");
    core::DrawList drawList;
    world.UpdateTransforms();
    world.ExtractDrawList(drawList);
    CHECK(drawList.joints.size() == 2 * hero->skeleton->joints.size()); // the player and the statue

    // 2. A rigged model without a graph still gets its rest pose; the others are reported.
    REQUIRE(world.GetAnimator(statue) != nullptr);
    CHECK(world.GetAnimator(statue)->State().empty());
    CHECK(world.GetAnimator(elderE) == nullptr);
    CHECK(world.GetAnimator(ghost) == nullptr);
    CHECK(Has(diagnostics, "entity.animation.skeleton"));
    CHECK(Has(diagnostics, "entity.animation.graph"));
}

TEST_CASE("Joint names match with or without a namespace prefix", "[animation]")
{
    core::Skeleton s;
    s.joints.resize(2);
    s.joints[0].name = "Hips";
    s.joints[1].name = "mixamorig:Spine";
    s.joints[1].parent = 0;
    CHECK(s.Find("Hips") == 0);
    CHECK(s.Find("mixamorig:Hips") == 0);
    CHECK(s.Find("Spine") == 1);
    CHECK(s.Find("Head") == -1);
}
