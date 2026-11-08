#include "assets/TestFiles.hpp"

#include <GamePack.hpp>
#include <WebPack.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>

using namespace umbra;

namespace
{
    std::string Read(const std::filesystem::path &file)
    {
        std::ifstream in(file, std::ios::binary);
        return {std::istreambuf_iterator<char>(in), {}};
    }
}

TEST_CASE("web-pack writes the assets the page loads, and a new version when they change (T76)")
{
    // A tiny project and a web build folder with a stamped sw.js.
    const std::filesystem::path root = test::TempDir("webpack");
    for (const char *folder : {"assets/rpg", "assets/scenes", "assets/textures", "web"})
        std::filesystem::create_directories(root / folder);
    test::WriteText(root / "assets/rpg/a.json", "{\"hp\": 1}");
    test::WriteText(root / "assets/scenes/b.scene.json", "{}");
    test::WriteText(root / "assets/textures/x.concept.png", "AI working file");
    test::WriteText(root / "web/umbra.js", "program");
    test::WriteText(root / "web/sw.js", "const VERSION = 'abc';\nconst FILES = [];\n");

    std::vector<core::Diagnostic> diagnostics;
    const std::string first = authoring::PackWebAssets(root / "assets", root / "web", diagnostics);
    REQUIRE(diagnostics.empty());
    REQUIRE(first.size() == 12);

    // Each file sits where assets.json says; AI working files are left out.
    const auto index = nlohmann::json::parse(Read(root / "web/assets.json"));
    const std::string bin = Read(root / "web/assets.bin");
    REQUIRE(index["files"].size() == 2);
    CHECK(index["size"] == bin.size());
    for (const auto &f : index["files"])
        CHECK(bin.substr(f["offset"], f["size"]) == Read(root / "assets" / f["path"].get<std::string>()));
    CHECK(Read(root / "web/sw.js").find("const VERSION = 'abc+" + first + "';") != std::string::npos);

    // Same files: same version. An edit: a new version (the offline copy updates), not appended twice.
    CHECK(authoring::PackWebAssets(root / "assets", root / "web", diagnostics) == first);
    test::WriteText(root / "assets/rpg/a.json", "{\"hp\": 999}");
    const std::string second = authoring::PackWebAssets(root / "assets", root / "web", diagnostics);
    CHECK(second != first);
    CHECK(Read(root / "web/sw.js").find("const VERSION = 'abc+" + second + "';") != std::string::npos);

    // Not a web build: an error, nothing written.
    CHECK(authoring::PackWebAssets(root / "assets", root / "nothing", diagnostics).empty());
    CHECK(core::HasErrors(diagnostics));
}

TEST_CASE("A phone's game pack: assets and shaders packed, unpacked once, bad entries refused (P11)")
{
    const std::filesystem::path root = test::TempDir("gamepack");
    for (const char *folder : {"assets/scenes", "assets/textures", "shaders"})
        std::filesystem::create_directories(root / folder);
    test::WriteText(root / "assets/scenes/b.scene.json", "{\"x\": 1}");
    test::WriteText(root / "assets/textures/x.concept.png", "AI working file");
    test::WriteText(root / "shaders/Scene.vert.spv", "vert");
    test::WriteText(root / "shaders/Scene.frag.spv", "frag");
    test::WriteText(root / "shaders/notes.txt", "not a shader");

    std::vector<core::Diagnostic> diagnostics;
    const std::string hash = authoring::PackGame(root / "assets", root / "shaders", root / "apk", diagnostics);
    REQUIRE(diagnostics.empty());
    const std::string index = Read(root / "apk/game.pack.json"), bin = Read(root / "apk/game.pack");
    CHECK(assets::PackHash(index) == hash);
    const auto files = nlohmann::json::parse(index)["files"];
    CHECK(files.size() == 3); // scene + 2 shaders; no concept picture, no .txt

    // First start: everything lands under the app folder, then the hash is recorded.
    const std::filesystem::path app = root / "app";
    CHECK(assets::InstalledPackHash(app).empty());
    std::string error;
    REQUIRE(assets::UnpackGame(index, std::as_bytes(std::span(bin)), app, error));
    CHECK(Read(app / "assets/scenes/b.scene.json") == "{\"x\": 1}");
    CHECK(Read(app / "shaders/Scene.frag.spv") == "frag");
    CHECK(assets::InstalledPackHash(app) == hash);

    // A pack whose entry leaves the folder, or points past the end, is refused.
    auto bad = nlohmann::json::parse(index);
    bad["files"][0]["path"] = "../outside.txt";
    CHECK_FALSE(assets::UnpackGame(bad.dump(), std::as_bytes(std::span(bin)), app, error));
    CHECK(assets::InstalledPackHash(app).empty()); // so the next start unpacks again
    bad = nlohmann::json::parse(index);
    bad["files"][0]["size"] = 1 << 20;
    CHECK_FALSE(assets::UnpackGame(bad.dump(), std::as_bytes(std::span(bin)), app, error));
    CHECK_FALSE(authoring::PackGame(root / "assets", root / "nowhere", root / "apk2", diagnostics).size() > 0);
}
