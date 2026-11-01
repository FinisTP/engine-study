#include <catch2/catch_test_macros.hpp>

#include <GltfImporter.hpp>
#include <ModelCache.hpp>

#include "TestFiles.hpp"

#include <cstring>

using namespace umbra::assets;

namespace
{
    const std::filesystem::path assets{UMBRA_TEST_ASSET_DIRECTORY};

    // Asset root containing a private copy of models/tower.gltf + tower.bin.
    std::filesystem::path TowerRoot(const std::string &name)
    {
        const auto root = test::TempDir(name);
        std::filesystem::create_directories(root / "models");
        std::filesystem::copy_file(assets / "models" / "tower.gltf", root / "models" / "tower.gltf");
        std::filesystem::copy_file(assets / "models" / "tower.bin", root / "models" / "tower.bin");
        return root;
    }

    void FlipByte(const std::filesystem::path &path, std::size_t offset)
    {
        std::string bytes = test::ReadText(path);
        bytes[offset] = static_cast<char>(bytes[offset] ^ 0x5A);
        test::WriteText(path, bytes);
    }
}

TEST_CASE("A model survives a write/read round trip")
{
    const auto imported = ImportGltf(assets / "models" / "tower.gltf");
    REQUIRE(imported.Ok());
    const auto file = test::TempDir("roundtrip") / "tower.umodel";
    std::string error;
    REQUIRE(WriteModelFile(file, imported.model, BuildInfo{42, {"tower.bin"}}, error));

    Model model;
    BuildInfo info;
    REQUIRE(ReadModelFile(file, model, info, error));
    REQUIRE(info.sourceHash == 42);
    REQUIRE(info.dependencies == std::vector<std::string>{"tower.bin"});
    REQUIRE(model.nodes.size() == imported.model.nodes.size());
    REQUIRE(model.nodes[3].name == "Top");
    REQUIRE(model.nodes[3].parent == 2);
    REQUIRE(model.nodes[3].rotation == imported.model.nodes[3].rotation);
    REQUIRE(model.meshes[0].data.indices == imported.model.meshes[0].data.indices);
    REQUIRE(std::memcmp(model.meshes[0].data.vertices.data(), imported.model.meshes[0].data.vertices.data(),
                        model.meshes[0].data.vertices.size() * sizeof(umbra::core::Vertex)) == 0);
    REQUIRE(model.textures[0].rgba == imported.model.textures[0].rgba);
}

TEST_CASE("Corrupt, truncated and foreign cache files are rejected")
{
    const auto imported = ImportGltf(assets / "models" / "tower.gltf");
    const auto dir = test::TempDir("corrupt");
    std::string error;
    REQUIRE(WriteModelFile(dir / "a.umodel", imported.model, BuildInfo{1, {}}, error));
    Model model;
    BuildInfo info;

    const std::string good = test::ReadText(dir / "a.umodel");
    FlipByte(dir / "a.umodel", good.size() / 2);
    REQUIRE_FALSE(ReadModelFile(dir / "a.umodel", model, info, error));

    test::WriteText(dir / "a.umodel", good.substr(0, good.size() - 20));
    REQUIRE_FALSE(ReadModelFile(dir / "a.umodel", model, info, error));

    test::WriteText(dir / "a.umodel", "definitely not a model file");
    REQUIRE_FALSE(ReadModelFile(dir / "a.umodel", model, info, error));

    // A different format version is refused even before the checksum is looked at.
    test::WriteText(dir / "a.umodel", good);
    FlipByte(dir / "a.umodel", 4);
    REQUIRE_FALSE(ReadModelInfo(dir / "a.umodel", info, error));
    REQUIRE(error.find("version") != std::string::npos);
}

TEST_CASE("Builds happen once and repeat only when the source or a dependency changes")
{
    const auto root = TowerRoot("incremental");
    const auto cache = root / "cache";

    auto result = BuildModel(root, "models/tower.gltf", cache);
    REQUIRE(result.status == BuildStatus::Built);
    REQUIRE(std::filesystem::exists(result.output));
    REQUIRE(BuildModel(root, "models/tower.gltf", cache).status == BuildStatus::UpToDate);

    // Change one byte of a vertex position in the external buffer (same size).
    FlipByte(root / "models" / "tower.bin", 1);
    REQUIRE(BuildModel(root, "models/tower.gltf", cache).status == BuildStatus::Built);
    REQUIRE(BuildModel(root, "models/tower.gltf", cache).status == BuildStatus::UpToDate);

    // Any edit of the source file itself.
    test::WriteText(root / "models" / "tower.gltf", test::ReadText(root / "models" / "tower.gltf") + "\n");
    REQUIRE(BuildModel(root, "models/tower.gltf", cache).status == BuildStatus::Built);
}

TEST_CASE("Repeated builds produce identical bytes")
{
    const auto root = TowerRoot("deterministic");
    const auto a = BuildModel(root, "models/tower.gltf", root / "cacheA");
    const auto b = BuildModel(root, "models/tower.gltf", root / "cacheB");
    REQUIRE(a.status == BuildStatus::Built);
    REQUIRE(test::ReadText(a.output) == test::ReadText(b.output));
}

TEST_CASE("A failed build keeps the last good cached file")
{
    const auto root = TowerRoot("keepgood");
    const auto cache = root / "cache";
    const auto first = BuildModel(root, "models/tower.gltf", cache);
    const std::string good = test::ReadText(first.output);

    // Half-written source (e.g. an editor still saving).
    test::WriteText(root / "models" / "tower.gltf", R"({"asset":{"version":"2.0"},"nod)");
    const auto failed = BuildModel(root, "models/tower.gltf", cache);
    REQUIRE(failed.status == BuildStatus::Failed);
    REQUIRE_FALSE(failed.errors.empty());
    REQUIRE(test::ReadText(first.output) == good);

    // Missing dependency.
    std::filesystem::copy_file(assets / "models" / "tower.gltf", root / "models" / "tower.gltf", std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove(root / "models" / "tower.bin");
    REQUIRE(BuildModel(root, "models/tower.gltf", cache).status == BuildStatus::Failed);
    REQUIRE(test::ReadText(first.output) == good);
}

TEST_CASE("Asset paths must stay inside the asset root")
{
    REQUIRE(IsSafeAssetPath("models/tower.gltf"));
    REQUIRE_FALSE(IsSafeAssetPath("../tower.gltf"));
    REQUIRE_FALSE(IsSafeAssetPath("models/../../x.gltf"));
    REQUIRE_FALSE(IsSafeAssetPath("/etc/x.gltf"));
    REQUIRE_FALSE(IsSafeAssetPath(""));
    REQUIRE(BuildModel(test::TempDir("unsafe"), "../x.gltf", "cache").status == BuildStatus::Failed);
}
