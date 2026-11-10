#include <catch2/catch_approx.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <GltfImporter.hpp>
#include <ModelCache.hpp>
#include <Skeleton.hpp>

#include "TestFiles.hpp"

#include <glm/gtc/constants.hpp>

using Catch::Approx;
using umbra::assets::ImportGltf;

namespace
{
    const std::filesystem::path assets{UMBRA_TEST_ASSET_DIRECTORY};

    umbra::assets::ImportResult ImportTriangle(const std::string &name, const test::TriangleGltf &g)
    {
        const auto dir = test::TempDir(name);
        test::WriteTriangleBin(dir / "tri.bin");
        test::WriteText(dir / "tri.gltf", test::TriangleJson(g));
        return ImportGltf(dir / "tri.gltf");
    }
}

TEST_CASE("Importer reads the tower hierarchy, mesh, texture and dependency")
{
    const auto result = ImportGltf(assets / "models" / "tower.gltf");
    REQUIRE(result.Ok());

    const auto &model = result.model;
    REQUIRE(model.nodes.size() == 4);
    REQUIRE(model.nodes[0].name == "Tower");
    REQUIRE(model.nodes[0].parent == -1);
    REQUIRE(model.nodes[1].name == "Base");
    REQUIRE(model.nodes[1].parent == 0);
    REQUIRE(model.nodes[3].name == "Top");
    REQUIRE(model.nodes[3].parent == 2);
    REQUIRE(model.nodes[0].position.z == Approx(-6.0F));
    REQUIRE(model.nodes[0].meshes.empty());
    REQUIRE(model.nodes[1].meshes.size() == 1);

    REQUIRE(model.meshes.size() == 1);
    REQUIRE(model.meshes[0].data.vertices.size() == 24);
    REQUIRE(model.meshes[0].data.indices.size() == 36);
    REQUIRE(model.meshes[0].texture == 0);
    REQUIRE(model.textures.size() == 1);
    REQUIRE(model.textures[0].rgba.size() == model.textures[0].width * model.textures[0].height * 4);

    REQUIRE(result.dependencies == std::vector<std::string>{"tower.bin"});
}

TEST_CASE("Importer reads the embedded phase 2 cube with no dependencies")
{
    const auto result = ImportGltf(assets / "scenes" / "phase2.gltf");
    REQUIRE(result.Ok());
    REQUIRE(result.dependencies.empty());
    REQUIRE(result.model.nodes.size() == 1);
}

TEST_CASE("Parents are ordered before children even when the file lists them later")
{
    test::TriangleGltf g;
    g.nodes = R"([{"name":"Child","mesh":0,"translation":[0,1,0]},{"name":"Root","children":[0]}])";
    const auto result = ImportTriangle("order", g);
    REQUIRE(result.Ok());
    REQUIRE(result.model.nodes[0].name == "Root");
    REQUIRE(result.model.nodes[1].name == "Child");
    REQUIRE(result.model.nodes[1].parent == 0);
}

TEST_CASE("A node matrix is split into translation, rotation and scale")
{
    // translate(1,2,3) * rotateY(90 degrees) * scale(2), column-major
    test::TriangleGltf g;
    g.nodes = R"([{"mesh":0,"matrix":[0,0,-2,0, 0,2,0,0, 2,0,0,0, 1,2,3,1]}])";
    const auto result = ImportTriangle("matrix", g);
    REQUIRE(result.Ok());

    const auto &node = result.model.nodes[0];
    REQUIRE(node.position.x == Approx(1.0F));
    REQUIRE(node.position.z == Approx(3.0F));
    REQUIRE(node.scale.x == Approx(2.0F));
    REQUIRE(node.scale.y == Approx(2.0F));
    const glm::vec3 x = node.rotation * glm::vec3{1, 0, 0};
    REQUIRE(x.z == Approx(-1.0F).margin(1e-5));
    REQUIRE(node.name.empty());
}

TEST_CASE("Non-indexed triangles get sequential indices")
{
    test::TriangleGltf g;
    g.indices = "";
    const auto result = ImportTriangle("noindices", g);
    REQUIRE(result.Ok());
    REQUIRE(result.model.meshes[0].data.indices == std::vector<std::uint32_t>{0, 1, 2});
}

TEST_CASE("Node cycles and shared children are rejected")
{
    test::TriangleGltf cycle;
    cycle.nodes = R"([{"mesh":0,"children":[1]},{"children":[0]}])";
    REQUIRE_FALSE(ImportTriangle("cycle", cycle).Ok());

    test::TriangleGltf shared;
    shared.nodes = R"([{"children":[2]},{"children":[2]},{"mesh":0}])";
    REQUIRE_FALSE(ImportTriangle("shared", shared).Ok());
}

TEST_CASE("An accessor reaching past its buffer is rejected")
{
    test::TriangleGltf g;
    g.positionCount = 1000;
    REQUIRE_FALSE(ImportTriangle("oob", g).Ok());
}

TEST_CASE("External paths leaving the model folder are rejected")
{
    const auto dir = test::TempDir("traversal");
    std::filesystem::create_directories(dir / "inner");
    test::WriteTriangleBin(dir / "tri.bin");
    test::TriangleGltf g;
    g.uri = "../tri.bin";
    test::WriteText(dir / "inner" / "tri.gltf", test::TriangleJson(g));
    REQUIRE_FALSE(ImportGltf(dir / "inner" / "tri.gltf").Ok());
}

TEST_CASE("Required extensions, missing and malformed files are reported as errors")
{
    test::TriangleGltf g;
    g.extra = R"("extensionsUsed":["KHR_unknown_x"],"extensionsRequired":["KHR_unknown_x"],)";
    REQUIRE_FALSE(ImportTriangle("extension", g).Ok());

    REQUIRE_FALSE(ImportGltf(test::TempDir("missing") / "nope.gltf").Ok());

    const auto dir = test::TempDir("truncated");
    test::WriteText(dir / "bad.gltf", R"({"asset":{"version":"2.0"},"nodes":[{)");
    const auto truncated = ImportGltf(dir / "bad.gltf");
    REQUIRE_FALSE(truncated.Ok());
    REQUIRE_FALSE(truncated.errors.front().empty());
}

TEST_CASE("A rigged character imports with its skeleton, skin and clips, and survives the cache (T87)", "[gltf][animation]")
{
    // models/hero.glb: Meshy rig packed by tools/models/pack_animated.py.
    const auto source = std::filesystem::path(UMBRA_TEST_ASSET_DIRECTORY) / "models/hero.glb";
    const umbra::assets::ImportResult imported = umbra::assets::ImportGltf(source);
    INFO((imported.errors.empty() ? std::string() : imported.errors[0]));
    REQUIRE(imported.errors.empty());
    const umbra::assets::Model &model = imported.model;
    REQUIRE(model.skeleton);
    CHECK(model.skeleton->joints.size() == 24);
    CHECK(model.skeleton->Find("Hips") >= 0);
    REQUIRE(model.meshes.size() == 1);
    const umbra::core::MeshData &mesh = model.meshes[0].data;
    REQUIRE(mesh.skin.size() == mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.skin.size(); i += 97)
    {
        const glm::vec4 w = mesh.skin[i].weights;
        CHECK(w.x + w.y + w.z + w.w == Catch::Approx(1.0F).margin(1e-4));
    }
    std::vector<std::string> names;
    for (const umbra::core::Clip &clip : model.clips)
        names.push_back(clip.name);
    CHECK(names == std::vector<std::string>{"idle", "walk", "run", "attack", "hit", "death"});
    CHECK(model.clips[1].duration > 0.5F);

    // At rest, the skinned mesh fits the -1..1 box the scenes expect (pack_animated.py's root).
    glm::vec3 lo(1e9F), hi(-1e9F);
    for (const glm::vec3 &p : umbra::core::RestPositions(*model.skeleton, mesh))
    {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    CHECK(hi.y == Catch::Approx(1.0F).margin(0.05F));
    CHECK(lo.y == Catch::Approx(-1.0F).margin(0.05F));

    // The .umodel cache keeps all of it.
    const auto dir = test::TempDir("skinned-cache");
    std::string error;
    REQUIRE(umbra::assets::WriteModelFile(dir / "hero.umodel", model, {1, {}}, error));
    umbra::assets::Model loaded;
    umbra::assets::BuildInfo info;
    REQUIRE(umbra::assets::ReadModelFile(dir / "hero.umodel", loaded, info, error));
    REQUIRE(loaded.skeleton);
    CHECK(loaded.skeleton->joints.size() == 24);
    CHECK(loaded.meshes[0].data.skin.size() == mesh.skin.size());
    CHECK(loaded.clips.size() == 6);
    CHECK(loaded.clips[3].channels.size() == model.clips[3].channels.size());
}
