#include "assets/TestFiles.hpp"

#include <Document.hpp>
#include <Validate.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace umbra;

namespace
{
    std::vector<std::string> Codes(const std::vector<core::Diagnostic> &diagnostics)
    {
        std::vector<std::string> codes;
        for (const core::Diagnostic &d : diagnostics)
            codes.push_back(d.code + " " + d.field);
        return codes;
    }
}

TEST_CASE("The tool and the editor report the same problems for the same file")
{
    // Roadmap P7.4: one invalid input, matching command-line and editor diagnostics.
    const std::filesystem::path broken = std::filesystem::path{UMBRA_TEST_DATA_DIRECTORY} / "broken.rpg.json";
    REQUIRE(authoring::DetectKind(broken) == authoring::FileKind::Rpg);

    std::vector<core::Diagnostic> tool;
    REQUIRE_FALSE(authoring::ValidateFile(broken, {}, tool));

    std::vector<core::Diagnostic> editor;
    authoring::Document doc(broken, authoring::RpgValidator());
    REQUIRE_FALSE(doc.Open(editor));

    REQUIRE_FALSE(tool.empty());
    REQUIRE(Codes(tool) == Codes(editor));
    REQUIRE(tool[0].code == "ref.item");
}

TEST_CASE("Shipped content passes; unknown files and saves without content are explained")
{
    const std::filesystem::path assets{UMBRA_TEST_ASSET_DIRECTORY};
    std::vector<core::Diagnostic> diagnostics;
    REQUIRE(authoring::ValidateFile(assets / "scenes/dungeon.scene.json", {assets, {}}, diagnostics));
    REQUIRE(authoring::ValidateFile(assets / "rpg/reference.rpg.json", {}, diagnostics));
    REQUIRE(diagnostics.empty());

    const auto dir = test::TempDir("validate-unknown");
    test::WriteText(dir / "notes.json", R"({"hello": 1})");
    REQUIRE_FALSE(authoring::ValidateFile(dir / "notes.json", {}, diagnostics));
    REQUIRE(diagnostics.back().code == "file.format");

    test::WriteText(dir / "a.sav", R"({"format": "umbra.save", "version": 1})");
    REQUIRE_FALSE(authoring::ValidateFile(dir / "a.sav", {}, diagnostics));
    REQUIRE(diagnostics.back().code == "save.content.missing");
}
