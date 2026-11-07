#include "assets/TestFiles.hpp"

#include <Package.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace umbra;

namespace
{
    // A minimal good package: the program, the reference content (copied), shaders and notices.
    std::filesystem::path GoodPackage(const std::string &name)
    {
        const std::filesystem::path root = test::TempDir(name);
        const std::filesystem::path assets(UMBRA_TEST_ASSET_DIRECTORY);
        test::WriteText(root / "Umbra.exe", "MZ program bytes, finds assets at run time");
        test::WriteText(root / "SDL3.dll", "dll");
        test::WriteText(root / "vcruntime140_1.dll", "dll");
        test::WriteText(root / "THIRD_PARTY_NOTICES.md", "| SDL3 | zlib | `SDL3.dll` |\n| runtime | `vcruntime140*.dll` |\n");
        std::filesystem::create_directories(root / "shaders");
        test::WriteText(root / "shaders/Scene.vert.spv", "spv");
        test::WriteText(root / "shaders/Scene.frag.spv", "spv");
        for (const char *file : {"rpg/reference.rpg.json", "scenes/settlement.scene.json", "scenes/dungeon.scene.json", "scenes/arena.scene.json",
                                 "policies/bandit_leader.policy.json"})
        {
            std::filesystem::create_directories((root / "assets" / file).parent_path());
            std::filesystem::copy_file(assets / file, root / "assets" / file);
        }
        return root;
    }

    bool Has(const std::vector<core::Diagnostic> &diagnostics, const std::string &code)
    {
        return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const core::Diagnostic &d) { return d.code == code; });
    }
}

TEST_CASE("A good package passes the release check (T75)")
{
    std::vector<core::Diagnostic> diagnostics;
    CHECK(authoring::CheckPackage(GoodPackage("package-good"), "E:/Projects/EngineStudy", diagnostics));
    CHECK_FALSE(core::HasErrors(diagnostics));
}

TEST_CASE("What may not ship is named (T75)")
{
    const std::filesystem::path root = GoodPackage("package-bad");
    std::vector<core::Diagnostic> d;
    SECTION("development programs and files")
    {
        test::WriteText(root / "UmbraEditor.exe", "x");
        test::WriteText(root / "make_sounds.py", "x");
        CHECK_FALSE(authoring::CheckPackage(root, "", d));
        CHECK(Has(d, "package.forbidden"));
    }
    SECTION("a library without a license notice")
    {
        test::WriteText(root / "mystery.dll", "x");
        CHECK_FALSE(authoring::CheckPackage(root, "", d));
        CHECK(Has(d, "package.license"));
    }
    SECTION("a key in a content file, never printed")
    {
        test::WriteText(root / "assets/settings.json", R"({"api_key": "msy_abcdefghijklmnop1234"})");
        CHECK_FALSE(authoring::CheckPackage(root, "", d));
        REQUIRE(Has(d, "package.secret"));
        for (const auto &x : d)
            CHECK(x.message.find("msy_") == std::string::npos);
    }
    SECTION("an absolute path in content")
    {
        test::WriteText(root / "assets/scenes/extra.scene.json", R"({"entities":[{"model":"E:\\Projects\\EngineStudy\\assets\\models\\x.glb"}]})");
        CHECK_FALSE(authoring::CheckPackage(root, "", d));
        CHECK(Has(d, "package.path"));
    }
    SECTION("the program depends on the source assets folder")
    {
        test::WriteText(root / "Umbra.exe", "MZ ... E:/Projects/EngineStudy/assets ... E:/Projects/EngineStudy/engine/core/x.cpp");
        CHECK_FALSE(authoring::CheckPackage(root, "E:/Projects/EngineStudy", d));
        CHECK(Has(d, "package.buildpath"));
    }
    SECTION("source file names alone are only a warning")
    {
        test::WriteText(root / "Umbra.exe", "MZ ... E:/Projects/EngineStudy/engine/core/x.cpp");
        CHECK(authoring::CheckPackage(root, "E:/Projects/EngineStudy", d));
        CHECK(Has(d, "package.sourcenames"));
    }
    SECTION("missing content and policy")
    {
        std::filesystem::remove(root / "assets/policies/bandit_leader.policy.json");
        std::filesystem::remove(root / "shaders/Scene.frag.spv");
        CHECK_FALSE(authoring::CheckPackage(root, "", d));
        CHECK(Has(d, "package.policy"));
        CHECK(Has(d, "package.content"));
    }
    SECTION("AI leftovers and non-commercial models are warnings")
    {
        std::filesystem::create_directories(root / "assets/models");
        test::WriteText(root / "assets/models/barrel.concept.png", "png");
        test::WriteText(root / "assets/models/barrel.source.json", R"({"workflow":"C:/x.json","models":[{"file":"x.safetensors","license":"unknown","commercial":false}]})");
        CHECK(authoring::CheckPackage(root, "", d));
        CHECK(Has(d, "package.extra"));
        CHECK(Has(d, "package.ai-license"));
    }
}
