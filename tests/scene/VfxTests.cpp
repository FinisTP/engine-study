#include <AnimGraph.hpp>
#include <SceneFile.hpp>
#include <Vfx.hpp>

#include "../assets/TestFiles.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>

using namespace umbra;

namespace
{
    const std::filesystem::path assetDir{UMBRA_TEST_ASSET_DIRECTORY};

    bool Has(const std::vector<core::Diagnostic> &diagnostics, const std::string &code)
    {
        return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const core::Diagnostic &d) { return d.code == code; });
    }

    std::vector<core::Diagnostic> Parse(const std::string &fields)
    {
        core::VfxEffect effect;
        std::vector<core::Diagnostic> diagnostics;
        scene::ParseVfx(R"({"format": "umbra.vfx", "version": 1)" + fields + "}", "test.vfx.json", effect, diagnostics);
        return diagnostics;
    }
}

TEST_CASE("The shipped effects load; mistakes are named by field (T89)", "[vfx]")
{
    for (const char *file : {"vfx/fire.vfx.json", "vfx/fireball.vfx.json", "vfx/hit.vfx.json", "vfx/poison.vfx.json", "vfx/dust.vfx.json", "vfx/heal.vfx.json"})
    {
        std::vector<core::Diagnostic> diagnostics;
        INFO(file);
        CHECK(scene::LoadVfx(assetDir / file, file, diagnostics));
        CHECK(diagnostics.empty());
    }
    std::vector<core::Diagnostic> diagnostics;
    const auto fire = scene::LoadVfx(assetDir / "vfx/fire.vfx.json", "vfx/fire.vfx.json", diagnostics);
    REQUIRE(fire);
    CHECK(fire->additive);
    CHECK(fire->rate == 45.0F);
    const auto poison = scene::LoadVfx(assetDir / "vfx/poison.vfx.json", "vfx/poison.vfx.json", diagnostics);
    REQUIRE(poison);
    CHECK_FALSE(poison->additive); // bubbles cover
    CHECK(poison->ring > 0.0F);

    core::VfxEffect effect;
    CHECK_FALSE(scene::ParseVfx(R"({"format": "umbra.scene", "version": 1})", "x", effect, diagnostics));
    CHECK(Has(diagnostics, "vfx.format"));
    CHECK(Parse("").empty()); // every field has a default
    CHECK(Has(Parse(R"(, "rate": -1)"), "vfx.rate"));
    CHECK(Has(Parse(R"(, "rate": 0)"), "vfx.rate")); // makes no particles
    CHECK(Parse(R"(, "rate": 0, "burst": 8)").empty());
    CHECK(Has(Parse(R"(, "lifetime": [2, 1])"), "vfx.lifetime"));
    CHECK(Has(Parse(R"(, "lifetime": 1)"), "vfx.lifetime"));
    CHECK(Has(Parse(R"(, "velocity": [0, 1])"), "vfx.velocity"));
    CHECK(Has(Parse(R"(, "color": [[1, 1, 1, 1]])"), "vfx.color"));
    CHECK(Has(Parse(R"(, "blend": "multiply")"), "vfx.blend"));
    CHECK(Has(Parse(R"(, "max": 100000)"), "vfx.max"));
    CHECK(Has(Parse(R"(, "ring": 1)"), "vfx.ring"));
    CHECK(scene::LoadVfx(assetDir / "vfx/none.vfx.json", "vfx/none.vfx.json", diagnostics) == nullptr);
    CHECK(Has(diagnostics, "vfx.read"));
}

TEST_CASE("The vfx component is read, saved again, and keeps an emitter at its entity (T89)", "[vfx]")
{
    scene::SceneLoadOptions options;
    options.assetRoot = assetDir;
    world::World w;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(scene::LoadSceneText(
        R"({"format":"umbra.scene","version":1,"entities":[
            {"id":"torch","position":[3,2,1],"components":{"vfx":{"effect":"vfx/fire.vfx.json"}}},
            {"id":"plain"}]})",
        "test.scene.json", options, w, diagnostics));
    const world::Entity torch = w.Find("torch");
    REQUIRE(w.GetVfx(torch) != nullptr);
    CHECK(*w.GetVfx(torch) == "vfx/fire.vfx.json");
    CHECK(w.GetVfx(w.Find("plain")) == nullptr);

    // 1. Saved as it was read.
    const auto dir = test::TempDir("vfx-save");
    REQUIRE(scene::SaveScene(w, dir / "s.scene.json", diagnostics));
    const nlohmann::json saved = nlohmann::json::parse(test::ReadText(dir / "s.scene.json"));
    CHECK(saved["entities"][0]["components"]["vfx"]["effect"] == "vfx/fire.vfx.json");
    CHECK_FALSE(saved["entities"][1].contains("components"));

    // 2. One emitter per entity, however often it is synced; the effect is asked for by path.
    w.UpdateTransforms();
    core::ParticleSystem particles;
    int asked = 0;
    const scene::VfxFinder finder = [&](const std::string &path) {
        ++asked;
        std::vector<core::Diagnostic> problems;
        static std::map<std::string, std::shared_ptr<const core::VfxEffect>> loaded; // loaded once, like the game
        if (!loaded.contains(path))
            loaded[path] = scene::LoadVfx(assetDir / path, path, problems);
        return loaded[path];
    };
    for (int i = 0; i < 30; ++i)
    {
        const std::vector<std::uint64_t> keys = scene::SyncVfx(w, particles, finder);
        REQUIRE(keys == std::vector<std::uint64_t>{scene::VfxKey(torch)});
        particles.KeepOnly(keys);
        particles.Update(1.0F / 30.0F);
    }
    CHECK(asked >= 1);
    CHECK(particles.Count() > 20);
    CHECK(particles.Count() <= 96); // the fire's own limit: one emitter, not thirty
    core::DrawList drawList;
    particles.Extract(drawList);
    REQUIRE_FALSE(drawList.particles.empty());
    CHECK(std::abs(drawList.particles[0].position.x - 3.0F) < 1.5F); // at the torch

    // 3. The component removed: the fire goes out.
    w.SetVfx(torch, "");
    particles.KeepOnly(scene::SyncVfx(w, particles, finder));
    for (int i = 0; i < 60; ++i)
        particles.Update(1.0F / 30.0F);
    CHECK(particles.Count() == 0);

    // 4. A vfx component without an effect file is refused.
    world::World bad;
    CHECK_FALSE(scene::LoadSceneText(R"({"format":"umbra.scene","version":1,"entities":[{"id":"t","components":{"vfx":{"effect":""}}}]})",
                                     "bad.scene.json", options, bad, diagnostics));
    CHECK(Has(diagnostics, "entity.vfx.effect"));
}

TEST_CASE("Graph events may name a sound and an effect; the shipped graphs time steps and hits (T89)", "[vfx]")
{
    for (const char *file : {"animations/humanoid.anim.json", "animations/bandit.anim.json", "animations/wolf.anim.json"})
    {
        std::vector<core::Diagnostic> diagnostics;
        const auto graph = scene::LoadAnimGraph(assetDir / file, file, diagnostics);
        INFO(file);
        REQUIRE(graph);
        int steps = 0, hits = 0;
        for (const core::AnimState &state : graph->states)
            for (const core::AnimEvent &e : state.events)
            {
                if (e.name == "step")
                {
                    ++steps;
                    CHECK(e.sound == "audio/footstep.wav");
                    CHECK(e.effect == "vfx/dust.vfx.json");
                }
                hits += e.name == "hit" ? 1 : 0;
            }
        CHECK(steps == 4); // two in the walk, two in the run
        CHECK(hits == 1);  // the attack lands once
    }
}
