#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <ComponentSchema.hpp>
#include <SceneFile.hpp>

#include "../assets/TestFiles.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>

using namespace umbra;

namespace
{
    const std::filesystem::path assetDir{UMBRA_TEST_ASSET_DIRECTORY};

    struct Loaded
    {
        bool ok = false;
        std::vector<core::Diagnostic> diagnostics;
        bool Has(const std::string &code) const
        {
            return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const core::Diagnostic &d) { return d.code == code; });
        }
    };

    // Loads entities into `world` with the repository's assets (its components/spin.component.json).
    Loaded Load(const std::string &entities, world::World &world)
    {
        scene::SceneLoadOptions options;
        options.assetRoot = assetDir;
        Loaded result;
        result.ok = scene::LoadSceneText(R"({"format":"umbra.scene","version":1,"entities":[)" + entities + "]}", "test.scene.json", options, world,
                                         result.diagnostics);
        return result;
    }
}

TEST_CASE("Entities hold built-in and project components; older files still load (T77)")
{
    // Built-in components inside "components" load like the older top-level form.
    world::World a, b;
    REQUIRE(Load(R"({"id":"sign","components":{"collider":{},"interactable":{"prompt":"Read"}}})", a).ok);
    REQUIRE(Load(R"({"id":"sign","collider":{},"interactable":{"prompt":"Read"}})", b).ok);
    CHECK(a.GetInteractable(a.Find("sign"))->prompt == "Read");
    CHECK(b.GetInteractable(b.Find("sign"))->prompt == "Read");
    CHECK(a.GetCollider(a.Find("sign")) != nullptr);

    // Both forms at once is an error.
    world::World c;
    CHECK(Load(R"({"id":"x","collider":{},"components":{"collider":{}}})", c).Has("entity.component.twice"));

    // A project component is stored for the game, checked against its schema.
    world::World d;
    const Loaded spin = Load(R"({"id":"stone","components":{"spin":{"speed":90}}})", d);
    REQUIRE(spin.ok);
    const auto *customs = d.GetCustoms(d.Find("stone"));
    REQUIRE(customs->size() == 1);
    CHECK((*customs)[0].type == "spin");
    CHECK(nlohmann::json::parse((*customs)[0].data)["speed"] == 90);
    world::World e;
    CHECK(Load(R"({"id":"s","components":{"spin":{"speed":"fast"}}})", e).Has("entity.component.field"));
    CHECK(Load(R"({"id":"s","components":{"spin":{"speed":5000}}})", e).Has("entity.component.field"));
    world::World f;
    const Loaded unknown = Load(R"({"id":"s","components":{"door":{"locked":true}}})", f);
    CHECK(unknown.ok); // only a warning: C++ may still register it
    CHECK(unknown.Has("entity.component.unknown"));

    // Saving writes every component under "components".
    const auto dir = test::TempDir("components");
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(scene::SaveScene(a, dir / "a.scene.json", diagnostics));
    REQUIRE(scene::SaveScene(d, dir / "d.scene.json", diagnostics));
    const auto savedA = nlohmann::json::parse(test::ReadText(dir / "a.scene.json"))["entities"][0];
    const auto savedD = nlohmann::json::parse(test::ReadText(dir / "d.scene.json"))["entities"][0];
    CHECK(savedA["components"].contains("collider"));
    CHECK(savedA["components"]["interactable"]["prompt"] == "Read");
    CHECK(!savedA.contains("collider"));
    CHECK(savedD["components"]["spin"]["speed"] == 90);
}

TEST_CASE("Component schemas give the editor's fields and defaults (T77)")
{
    std::vector<core::Diagnostic> diagnostics;
    const auto schemas = scene::LoadComponentSchemas(assetDir / "components", diagnostics);
    REQUIRE(diagnostics.empty());
    REQUIRE(schemas.contains("spin"));
    CHECK(schemas.at("spin").Defaults() == nlohmann::json{{"speed", 45}});

    // A schema may not take a built-in name or use an unknown field kind.
    const auto dir = test::TempDir("schemas");
    test::WriteText(dir / "a.component.json", R"({"format":"umbra.component","type":"collider"})");
    test::WriteText(dir / "b.component.json", R"({"format":"umbra.component","type":"door","fields":{"x":{"kind":"colour"}}})");
    diagnostics.clear();
    CHECK(scene::LoadComponentSchemas(dir, diagnostics).empty());
    CHECK(diagnostics.size() == 2);
}

TEST_CASE("Light and environment components are read, checked and saved again (T84)")
{
    world::World w;
    const Loaded ok = Load(R"({"id":"torch","position":[1,2,3],"components":{"light":{"intensity":5,"flicker":0.5}}},
                              {"id":"sky","components":{"environment":{"hour":19,"dayLength":600,"indoor":false,"tint":[1,0.9,0.8]}}},
                              {"id":"flame","model":"scenes/phase2.gltf","emissive":2})",
                           w);
    REQUIRE(ok.ok);
    const world::Light *light = w.GetLight(w.Find("torch"));
    REQUIRE(light != nullptr);
    CHECK(light->intensity == 5.0F);
    CHECK(light->flicker == 0.5F);
    CHECK(light->range == 8.0F); // a default for a missing field
    CHECK(w.GetEnvironment().dayLength == 600.0F);
    CHECK(w.GetEnvironment().tint == glm::vec3{1.0F, 0.9F, 0.8F});
    CHECK(w.Hour() == 19.0F);
    CHECK(w.EnvironmentOwner() == w.Find("sky"));
    CHECK(w.GetRenderable(w.Find("flame"))->emissive == 2.0F);
    // Not stored as project components: the game would warn that no C++ handles them.
    CHECK(w.GetCustoms(w.Find("torch"))->empty());

    // Extraction: the light at its world position, the environment of the current hour.
    w.UpdateTransforms();
    w.AdvanceTime(30.0F); // 600 s per day: 30 s = 1.2 h
    CHECK(w.Hour() == Catch::Approx(20.2F));
    core::DrawList drawList;
    w.ExtractDrawList(drawList);
    REQUIRE(drawList.lights.size() == 1);
    CHECK(drawList.lights[0].position == glm::vec3{1.0F, 2.0F, 3.0F});
    CHECK(drawList.environment.sunColor == core::EvaluateEnvironment(w.GetEnvironment(), w.Hour()).sunColor);

    // Saved with the current hour, so a checkpoint comes back at the same time of day.
    const auto dir = test::TempDir("lighting-save");
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(scene::SaveScene(w, dir / "s.scene.json", diagnostics));
    const nlohmann::json saved = nlohmann::json::parse(test::ReadText(dir / "s.scene.json"));
    CHECK(saved["entities"][0]["components"]["light"]["intensity"] == 5);
    CHECK(saved["entities"][1]["components"]["environment"]["hour"].get<double>() == Catch::Approx(20.2));
    CHECK(saved["entities"][2]["emissive"] == 2);
}

TEST_CASE("Bad light values are refused; a second environment is ignored with a warning (T84)")
{
    world::World a, b, c;
    CHECK_FALSE(Load(R"({"id":"x","components":{"light":{"intensity":-1}}})", a).ok);
    const Loaded color = Load(R"({"id":"x","components":{"light":{"color":[1,0]}}})", b);
    CHECK_FALSE(color.ok);
    CHECK(color.Has("entity.component.field"));
    const Loaded twice = Load(R"({"id":"a","components":{"environment":{"hour":6}}},{"id":"b","components":{"environment":{"hour":20}}})", c);
    CHECK(twice.ok);
    CHECK(twice.Has("scene.environment.twice"));
    CHECK(c.Hour() == 6.0F);
    world::World d;
    CHECK_FALSE(Load(R"({"id":"x","model":"scenes/phase2.gltf","emissive":"bright"})", d).ok);
}

TEST_CASE("The engine's light and environment are listed with the project's components (T84)")
{
    std::vector<core::Diagnostic> diagnostics;
    const auto schemas = scene::LoadComponentSchemas(assets::AssetFolders(assetDir), diagnostics);
    REQUIRE(schemas.contains("light"));
    REQUIRE(schemas.contains("environment"));
    CHECK(schemas.at("light").Defaults()["range"] == 8.0);
    CHECK(schemas.at("environment").Defaults()["shadows"] == true);
    CHECK(schemas.contains("spin")); // the project's own still there

    // A project may not take an engine component's name.
    const auto dir = test::TempDir("light-schema");
    std::filesystem::create_directories(dir / "components");
    test::WriteText(dir / "components" / "light.component.json", R"({"format":"umbra.component","version":1,"type":"light","fields":{}})");
    std::vector<core::Diagnostic> problems;
    (void)scene::LoadComponentSchemas(dir / "components", problems);
    REQUIRE(problems.size() == 1);
    CHECK(problems[0].code == "component.schema");
}

TEST_CASE("The post component is read with its LUT image, checked and saved again (T85)")
{
    world::World w;
    const Loaded ok = Load(R"({"id":"look","components":{"post":{"exposure":0.5,"temperature":-0.4,"lut":"textures/lut_moonlight.png"}}})", w);
    REQUIRE(ok.ok);
    CHECK(w.PostOwner() == w.Find("look"));
    CHECK(w.GetPost().exposure == 0.5F);
    CHECK(w.GetPost().bloom == 0.5F); // default
    core::DrawList drawList;
    w.UpdateTransforms();
    w.ExtractDrawList(drawList);
    CHECK(drawList.post.temperature == -0.4F);
    REQUIRE(drawList.postLut != nullptr);
    CHECK(drawList.postLut->width == 256);
    CHECK(w.GetCustoms(w.Find("look"))->empty());

    const auto dir = test::TempDir("post-save");
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(scene::SaveScene(w, dir / "s.scene.json", diagnostics));
    const nlohmann::json saved = nlohmann::json::parse(test::ReadText(dir / "s.scene.json"));
    CHECK(saved["entities"][0]["components"]["post"]["lut"] == "textures/lut_moonlight.png");
    CHECK(saved["entities"][0]["components"]["post"]["exposure"] == 0.5);

    // A LUT of the wrong size, a missing one, a bad value, a second post.
    world::World a, b, c, d;
    const Loaded size = Load(R"({"id":"x","components":{"post":{"lut":"textures/sample_bricks.png"}}})", a);
    CHECK_FALSE(size.ok);
    CHECK(size.Has("entity.post.lut"));
    CHECK_FALSE(Load(R"({"id":"x","components":{"post":{"lut":"textures/none.png"}}})", b).ok);
    CHECK_FALSE(Load(R"({"id":"x","components":{"post":{"bloom":9}}})", c).ok);
    const Loaded twice = Load(R"({"id":"a","components":{"post":{"exposure":1}}},{"id":"b","components":{"post":{"exposure":2}}})", d);
    CHECK(twice.ok);
    CHECK(twice.Has("scene.post.twice"));
    CHECK(d.GetPost().exposure == 1.0F);
}
