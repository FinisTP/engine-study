#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <GltfImporter.hpp>
#include <Checkpoint.hpp>
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
        options.assetRoot = test::ReferenceAssets(); // with the plugins' assets (sounds)
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

    REQUIRE(world.Count() == 1 + 6 + 2 + 1 + 4 + 3); // ground, ring, spinner+moon, tower anchor, 4 tower nodes, crates
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

TEST_CASE("Colliders load, validate and survive save/load")
{
    const auto dir = test::TempDir("scene-collider");
    test::WriteText(dir / "c.scene.json", R"({"format":"umbra.scene","version":1,"entities":[
        {"id":"floor","scale":[10,1,10],"collider":{}},
        {"id":"crate","collider":{"shape":"box","motion":"dynamic","halfExtents":[0.5,0.5,0.5]}},
        {"id":"ball","collider":{"shape":"sphere","motion":"kinematic","radius":0.25}}]})");
    world::World world;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(scene::LoadScene(dir / "c.scene.json", Options(), world, diagnostics));
    REQUIRE(world.GetCollider(world.Find("floor"))->motion == world::ColliderMotion::Static);
    REQUIRE(world.GetCollider(world.Find("crate"))->halfExtents.y == Approx(0.5F));
    REQUIRE(world.GetCollider(world.Find("ball"))->shape == world::ColliderShape::Sphere);

    REQUIRE(scene::SaveScene(world, dir / "saved.scene.json", diagnostics));
    world::World copy;
    REQUIRE(scene::LoadScene(dir / "saved.scene.json", Options(), copy, diagnostics));
    REQUIRE(copy.GetCollider(copy.Find("ball"))->radius == Approx(0.25F));
    REQUIRE(copy.GetCollider(copy.Find("ball"))->motion == world::ColliderMotion::Kinematic);
}

TEST_CASE("Bad colliders are rejected")
{
    REQUIRE(LoadEntities("col-shape", R"({"id":"a","collider":{"shape":"cone"}})").Has("entity.collider.shape"));
    REQUIRE(LoadEntities("col-motion", R"({"id":"a","collider":{"motion":"flying"}})").Has("entity.collider.motion"));
    REQUIRE(LoadEntities("col-size", R"({"id":"a","collider":{"radius":-1}})").Has("entity.collider.size"));
    REQUIRE(LoadEntities("col-type", R"({"id":"a","collider":{"shape":3}})").Has("entity.field.type"));
    REQUIRE(LoadEntities("col-child", R"({"id":"p"},{"id":"c","parent":"p","collider":{"motion":"dynamic"}})").Has("entity.collider.dynamicchild"));
}

TEST_CASE("Interactables load and survive save/load")
{
    const auto dir = test::TempDir("scene-interactable");
    test::WriteText(dir / "i.scene.json", R"({"format":"umbra.scene","version":1,"entities":[
        {"id":"sign","collider":{},"interactable":{"prompt":"Read sign","message":"Hello","sound":"audio/interact.wav"}},
        {"id":"well","collider":{},"interactable":{"prompt":"Look"}}]})");
    world::World world;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(scene::LoadScene(dir / "i.scene.json", Options(), world, diagnostics));
    REQUIRE(diagnostics.empty());
    REQUIRE(world.GetInteractable(world.Find("sign"))->message == "Hello");
    REQUIRE(world.GetInteractable(world.Find("well"))->sound.empty());

    REQUIRE(scene::SaveScene(world, dir / "saved.scene.json", diagnostics));
    world::World copy;
    REQUIRE(scene::LoadScene(dir / "saved.scene.json", Options(), copy, diagnostics));
    REQUIRE(copy.GetInteractable(copy.Find("sign"))->sound == "audio/interact.wav");
    REQUIRE(copy.GetInteractable(copy.Find("well"))->prompt == "Look");
}

TEST_CASE("Interactable problems: missing prompt fails, missing sound or collider only warns")
{
    REQUIRE(LoadEntities("int-prompt", R"({"id":"a","collider":{},"interactable":{}})").Has("entity.interactable.prompt"));
    REQUIRE(LoadEntities("int-unsafe", R"({"id":"a","collider":{},"interactable":{"prompt":"x","sound":"../x.wav"}})").Has("asset.path.unsafe"));

    const Attempt silent = LoadEntities("int-sound", R"({"id":"a","collider":{},"interactable":{"prompt":"x","sound":"audio/missing.wav"}})");
    REQUIRE(silent.loaded);
    REQUIRE(silent.Has("entity.interactable.sound"));

    const Attempt unreachable = LoadEntities("int-nocol", R"({"id":"a","interactable":{"prompt":"x"}})");
    REQUIRE(unreachable.loaded);
    REQUIRE(unreachable.Has("entity.interactable.nocollider"));
}

TEST_CASE("Portals load, validate and survive save/load")
{
    const auto dir = test::TempDir("scene-portal");
    test::WriteText(dir / "p.scene.json", R"({"format":"umbra.scene","version":1,"entities":[
        {"id":"gate","portal":{"scene":"scenes/dungeon.scene.json","spawn":"entry"}},
        {"id":"back","portal":{"scene":"scenes/settlement.scene.json"}}]})");
    world::World world;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(scene::LoadScene(dir / "p.scene.json", Options(), world, diagnostics));
    REQUIRE(world.GetPortal(world.Find("gate"))->spawn == "entry");
    REQUIRE(world.GetPortal(world.Find("back"))->spawn == "player"); // default

    REQUIRE(scene::SaveScene(world, dir / "saved.scene.json", diagnostics));
    world::World copy;
    REQUIRE(scene::LoadScene(dir / "saved.scene.json", Options(), copy, diagnostics));
    REQUIRE(copy.GetPortal(copy.Find("gate"))->scene == "scenes/dungeon.scene.json");

    REQUIRE(LoadEntities("portal-noscene", R"({"id":"a","portal":{}})").Has("entity.portal.scene"));
    REQUIRE(LoadEntities("portal-missing", R"({"id":"a","portal":{"scene":"scenes/nowhere.scene.json"}})").Has("asset.missing"));
    REQUIRE(LoadEntities("portal-spawn", R"({"id":"a","portal":{"scene":"scenes/dungeon.scene.json","spawn":""}})").Has("entity.portal.spawn"));
}

TEST_CASE("Both shipped areas load and link to each other")
{
    for (const char *name : {"settlement", "dungeon"})
    {
        world::World world;
        std::vector<core::Diagnostic> diagnostics;
        INFO(name);
        REQUIRE(scene::LoadScene(assetDir / "scenes" / (std::string{name} + ".scene.json"), Options(), world, diagnostics));
        REQUIRE(diagnostics.empty());
    }
}

TEST_CASE("Checkpoints restore the world and player, and repeated round trips do not grow")
{
    const auto dir = test::TempDir("scene-checkpoint");
    world::World original;
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(scene::LoadScene(assetDir / "scenes" / "settlement.scene.json", Options(), original, diagnostics));
    // Move a crate as if physics had pushed it.
    world::Transform moved = *original.GetLocalTransform(original.Find("crate_0"));
    moved.position = {1.0F, 0.4F, -7.0F};
    original.SetLocalTransform(original.Find("crate_0"), moved);
    REQUIRE(scene::SaveCheckpoint(original, {"scenes/settlement.scene.json", {2.0F, 0.0F, -1.0F}, 1.5F}, dir, diagnostics));
    const std::string firstSave = test::ReadText(dir / "world.scene.json");

    // Load -> save -> load ... : same entities, same file, every time (no duplicated instances).
    for (int i = 0; i < 10; ++i)
    {
        world::World restored;
        scene::Checkpoint checkpoint;
        REQUIRE(scene::LoadCheckpoint(dir, Options(), restored, checkpoint, diagnostics));
        REQUIRE(restored.Count() == original.Count());
        REQUIRE(restored.GetLocalTransform(restored.Find("crate_0"))->position.x == Approx(1.0F));
        REQUIRE(checkpoint.scene == "scenes/settlement.scene.json");
        REQUIRE(checkpoint.feet.z == Approx(-1.0F));
        REQUIRE(checkpoint.yaw == Approx(1.5F));
        REQUIRE(scene::SaveCheckpoint(restored, checkpoint, dir, diagnostics));
        REQUIRE(test::ReadText(dir / "world.scene.json") == firstSave);
    }
    REQUIRE_FALSE(core::HasErrors(diagnostics));
}

TEST_CASE("Missing or broken checkpoints are reported and change nothing")
{
    const auto dir = test::TempDir("scene-checkpoint-bad");
    world::World world;
    scene::Checkpoint checkpoint{"keep", {}, 0.0F};
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE_FALSE(scene::LoadCheckpoint(dir, Options(), world, checkpoint, diagnostics));
    REQUIRE(diagnostics.back().code == "checkpoint.read");

    test::WriteText(dir / "checkpoint.json", R"({"format":"umbra.checkpoint","version":1,"scene":"x","feet":[0,"a",0],"yaw":0})");
    REQUIRE_FALSE(scene::LoadCheckpoint(dir, Options(), world, checkpoint, diagnostics));

    test::WriteText(dir / "checkpoint.json", R"({"format":"umbra.checkpoint","version":1,"scene":"x","feet":[0,0,0],"yaw":0})");
    REQUIRE_FALSE(scene::LoadCheckpoint(dir, Options(), world, checkpoint, diagnostics)); // no world.scene.json
    REQUIRE(world.Count() == 0);
    REQUIRE(checkpoint.scene == "keep");
}

#include <Material.hpp>
#include <ModelInstance.hpp>
#include <TextureTools.hpp>

#include <fstream>

namespace
{
    // An asset folder with the cube model and one material whose image is `size` pixels square.
    std::filesystem::path MaterialAssets(const std::string &name, std::uint32_t size)
    {
        const auto root = test::TempDir("scene-material-" + name);
        std::filesystem::create_directories(root / "scenes");
        std::filesystem::copy_file(assetDir / "scenes/phase2.gltf", root / "scenes/phase2.gltf");
        core::TextureData image{size, size, std::vector<std::uint8_t>(std::size_t{size} * size * 4, 200)};
        std::vector<std::uint8_t> png;
        REQUIRE(assets::EncodePng(image, png));
        std::filesystem::create_directories(root / "textures");
        std::ofstream(root / "textures/stone.png", std::ios::binary).write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
        assets::Material material;
        material.baseColor = "textures/stone.png";
        material.worldProjection = true;
        std::filesystem::create_directories(root / "materials");
        test::WriteText(root / "materials/stone.material.json", assets::MaterialText(material));
        return root;
    }

    bool LoadIn(const std::filesystem::path &root, const std::string &entity, world::World &world, std::vector<core::Diagnostic> &diagnostics)
    {
        test::WriteText(root / "test.scene.json", R"({"format":"umbra.scene","version":1,"entities":[)" + entity + "]}");
        scene::SceneLoadOptions options;
        options.assetRoot = root;
        return scene::LoadScene(root / "test.scene.json", options, world, diagnostics);
    }

    bool HasCode(const std::vector<core::Diagnostic> &diagnostics, const std::string &code)
    {
        for (const auto &d : diagnostics)
            if (d.code == code)
                return true;
        return false;
    }
}

TEST_CASE("An entity's material is loaded, checked and saved back (P8C.1)")
{
    const auto root = MaterialAssets("ok", 128);
    world::World world;
    std::vector<core::Diagnostic> diagnostics;
    const bool loaded = LoadIn(root, R"({"id":"wall","model":"scenes/phase2.gltf","material":"materials/stone.material.json"})", world, diagnostics);
    for (const auto &d : diagnostics)
        UNSCOPED_INFO(core::Format(d));
    REQUIRE(loaded);
    const world::Renderable *renderable = world.GetRenderable(world.Find("wall"));
    REQUIRE(renderable != nullptr);
    CHECK(renderable->material == "materials/stone.material.json");

    // Binding: the material's texture and scale replace the model's.
    scene::BindRenderables(
        world, [](const std::string &, std::uint32_t) { return std::pair{core::MeshHandle{1, 1}, core::TextureHandle{7, 1}}; },
        [](const std::string &) { return std::optional<scene::MaterialBinding>{{core::TextureHandle{9, 1}, glm::vec4{1.0F}, 2.0F}}; });
    CHECK(renderable->textureHandle == core::TextureHandle{9, 1});
    CHECK(renderable->metersPerRepeat == 2.0F);
    core::DrawList drawList;
    world.ExtractDrawList(drawList);
    REQUIRE(drawList.items.size() == 1);
    CHECK(drawList.items[0].metersPerRepeat == 2.0F);

    REQUIRE(scene::SaveScene(world, root / "saved.scene.json", diagnostics));
    CHECK(test::ReadText(root / "saved.scene.json").find("\"material\": \"materials/stone.material.json\"") != std::string::npos);
}

TEST_CASE("Bad material references are refused with the material's own message (T63)")
{
    SECTION("missing file")
    {
        const auto root = MaterialAssets("missing", 128);
        world::World world;
        std::vector<core::Diagnostic> diagnostics;
        CHECK_FALSE(LoadIn(root, R"({"id":"wall","model":"scenes/phase2.gltf","material":"materials/none.material.json"})", world, diagnostics));
        CHECK(HasCode(diagnostics, "asset.missing"));
        CHECK(world.Count() == 0);
    }
    SECTION("image not a power of two")
    {
        const auto root = MaterialAssets("odd", 100);
        world::World world;
        std::vector<core::Diagnostic> diagnostics;
        CHECK_FALSE(LoadIn(root, R"({"id":"wall","model":"scenes/phase2.gltf","material":"materials/stone.material.json"})", world, diagnostics));
        REQUIRE(HasCode(diagnostics, "material.image"));
        CHECK(diagnostics.back().field == "entities[0].material");
    }
    SECTION("material without a model")
    {
        const auto root = MaterialAssets("nomodel", 128);
        world::World world;
        std::vector<core::Diagnostic> diagnostics;
        CHECK_FALSE(LoadIn(root, R"({"id":"wall","material":"materials/stone.material.json"})", world, diagnostics));
        CHECK(HasCode(diagnostics, "entity.material.nomodel"));
    }
}
