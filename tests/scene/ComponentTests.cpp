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
