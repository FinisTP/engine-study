#include "assets/TestFiles.hpp"

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
