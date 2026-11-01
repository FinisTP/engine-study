#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <GltfImporter.hpp>
#include <SceneFile.hpp>

#include "../assets/TestFiles.hpp"

#include <map>

using Catch::Approx;
using namespace umbra;

namespace
{
    const std::filesystem::path assetDir{UMBRA_TEST_ASSET_DIRECTORY};

    // Options that resolve models straight from the repository's asset folder.
    scene::SceneLoadOptions Options()
    {
        static std::map<std::string, assets::ImportResult> cache;
        scene::SceneLoadOptions options;
        options.assetRoot = assetDir;
        options.models = [](const std::string &path) -> const assets::Model * {
            auto it = cache.find(path);
            if (it == cache.end())
                it = cache.emplace(path, assets::ImportGltf(assetDir / path)).first;
            return it->second.Ok() ? &it->second.model : nullptr;
        };
        return options;
    }

    // Writes `entities` (a JSON array body) into a version-1 scene and tries to load it.
    struct Attempt
    {
        bool loaded = false;
        std::vector<core::Diagnostic> diagnostics;
        std::size_t entityCount = 0;

        bool Has(const std::string &code) const
        {
            for (const auto &d : diagnostics)
                if (d.code == code)
                    return true;
            return false;
        }
    };

    Attempt LoadText(const std::string &name, const std::string &text)
    {
        const auto file = test::TempDir("scene-" + name) / "test.scene.json";
        test::WriteText(file, text);
        world::World world;
        Attempt attempt;
        attempt.loaded = scene::LoadScene(file, Options(), world, attempt.diagnostics);
        attempt.entityCount = world.Count();
        return attempt;
    }

    Attempt LoadEntities(const std::string &name, const std::string &entities)
    {
        return LoadText(name, R"({"format":"umbra.scene","version":1,"entities":[)" + entities + "]}");
    }
}

TEST_CASE("The sample scene loads with hierarchy, renderables and an expanded instance")
{
    world::World world;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(scene::LoadScene(assetDir / "scenes" / "sample.scene.json", Options(), world, diagnostics));
    REQUIRE(diagnostics.empty());

    REQUIRE(world.Count() == 6 + 2 + 1 + 4); // ring, spinner+moon, tower anchor, 4 tower nodes
    REQUIRE(world.GetParent(world.Find("moon")) == world.Find("spinner"));
    REQUIRE(world.GetRenderable(world.Find("moon"))->color.g == Approx(0.8F));
    REQUIRE(world.GetParent(world.Find("tower/Tower")) == world.Find("tower"));
    REQUIRE(world.GetParent(world.Find("tower/Top")) == world.Find("tower/Middle"));
    REQUIRE(world.GetRenderable(world.Find("tower/Base"))->model == "models/tower.gltf");
}

TEST_CASE("Save then load reproduces the world, and saving again is byte-identical")
{
    world::World original;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(scene::LoadScene(assetDir / "scenes" / "sample.scene.json", Options(), original, diagnostics));

    const auto dir = test::TempDir("scene-roundtrip");
    REQUIRE(scene::SaveScene(original, dir / "a.scene.json", diagnostics));

    world::World copy;
    REQUIRE(scene::LoadScene(dir / "a.scene.json", Options(), copy, diagnostics));
    REQUIRE(diagnostics.empty());
    REQUIRE(copy.Count() == original.Count());
    original.ForEach([&](world::Entity entity) {
        const std::string &id = *original.GetId(entity);
        const world::Entity other = copy.Find(id);
        REQUIRE(other);
        const world::Entity parent = original.GetParent(entity);
        REQUIRE((parent ? *original.GetId(parent) : "") == (copy.GetParent(other) ? *copy.GetId(copy.GetParent(other)) : ""));
        REQUIRE(copy.GetLocalTransform(other)->position.x == Approx(original.GetLocalTransform(entity)->position.x).margin(1e-5));
        REQUIRE(copy.GetLocalTransform(other)->rotation.y == Approx(original.GetLocalTransform(entity)->rotation.y).margin(1e-5));
        REQUIRE((copy.GetRenderable(other) == nullptr) == (original.GetRenderable(entity) == nullptr));
    });

    REQUIRE(scene::SaveScene(copy, dir / "b.scene.json", diagnostics));
    REQUIRE(test::ReadText(dir / "a.scene.json") == test::ReadText(dir / "b.scene.json"));
}

TEST_CASE("Header problems are reported and nothing is created")
{
    REQUIRE(LoadText("json", R"({"format":"umbra.scene","version":1,"entities":[{"id":)").Has("scene.parse"));
    REQUIRE(LoadText("format", R"({"format":"other","version":1,"entities":[]})").Has("scene.format"));
    const Attempt newer = LoadText("version", R"({"format":"umbra.scene","version":2,"entities":[]})");
    REQUIRE(newer.Has("scene.version"));
    REQUIRE_FALSE(newer.loaded);
    REQUIRE(LoadText("entities", R"({"format":"umbra.scene","version":1})").Has("scene.entities"));
}

TEST_CASE("Duplicate ids, missing parents and cycles are rejected with field paths")
{
    const Attempt duplicate = LoadEntities("dup", R"({"id":"a"},{"id":"a"})");
    REQUIRE(duplicate.Has("entity.id.duplicate"));
    REQUIRE(duplicate.entityCount == 0);

    const Attempt missing = LoadEntities("parent", R"({"id":"a"},{"id":"b","parent":"ghost"})");
    REQUIRE(missing.Has("entity.parent.missing"));
    REQUIRE(missing.diagnostics.front().field == "entities[1].parent");

    const Attempt cycle = LoadEntities("cycle", R"({"id":"a","parent":"b"},{"id":"b","parent":"a"},{"id":"c"})");
    REQUIRE(cycle.Has("entity.parent.cycle"));
    REQUIRE(cycle.diagnostics.front().message.find("a -> b -> a") != std::string::npos);
    REQUIRE(cycle.entityCount == 0);
}

TEST_CASE("Bad field values are rejected")
{
    REQUIRE(LoadEntities("noid", R"({"name":"x"})").Has("entity.id.missing"));
    REQUIRE(LoadEntities("type", R"({"id":"a","position":[1,2]})").Has("entity.field.type"));
    REQUIRE(LoadEntities("zero", R"({"id":"a","rotation":[0,0,0,0]})").Has("entity.rotation.zero"));
    REQUIRE(LoadEntities("mesh", R"({"id":"a","mesh":1})").Has("entity.mesh.nomodel"));
    REQUIRE(LoadEntities("negative", R"({"id":"a","model":"scenes/phase2.gltf","mesh":-1})").Has("entity.field.type"));
    REQUIRE_FALSE(LoadEntities("huge", R"({"id":"a","position":[1e999,0,0]})").loaded);
}

TEST_CASE("Asset references must be safe and must exist")
{
    REQUIRE(LoadEntities("unsafe", R"({"id":"a","model":"../secret.gltf"})").Has("asset.path.unsafe"));
    REQUIRE(LoadEntities("absolute", R"({"id":"a","model":"/etc/x.gltf"})").Has("asset.path.unsafe"));
    REQUIRE(LoadEntities("missing", R"({"id":"a","model":"models/nope.gltf"})").Has("asset.missing"));
    REQUIRE(LoadEntities("instance", R"({"id":"a","instance":"scenes/sample.scene.json"})").Has("instance.failed"));
}

TEST_CASE("Unknown fields only warn, and rotations are normalized")
{
    const auto file = test::TempDir("scene-warn") / "w.scene.json";
    test::WriteText(file, R"({"format":"umbra.scene","version":1,"entities":[{"id":"a","colour":[1,0,0,1],"rotation":[0,0,0,2]}]})");
    world::World world;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(scene::LoadScene(file, Options(), world, diagnostics));
    REQUIRE(diagnostics.size() == 1);
    REQUIRE(diagnostics[0].severity == core::Severity::Warning);
    REQUIRE(diagnostics[0].code == "entity.field.unknown");
    REQUIRE(world.GetLocalTransform(world.Find("a"))->rotation.w == Approx(1.0F));
}

TEST_CASE("Loading refuses a non-empty world and validation creates nothing")
{
    world::World world;
    (void)world.Create("existing");
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE_FALSE(scene::LoadScene(assetDir / "scenes" / "sample.scene.json", Options(), world, diagnostics));
    REQUIRE(diagnostics.front().code == "scene.target");
    REQUIRE(world.Count() == 1);

    diagnostics.clear();
    REQUIRE(scene::ValidateScene(assetDir / "scenes" / "sample.scene.json", Options(), diagnostics));
    REQUIRE(diagnostics.empty());
}
