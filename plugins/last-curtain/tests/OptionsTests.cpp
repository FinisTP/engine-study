#include "NightTestData.hpp"

#include <Options.hpp>
#include <Play.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

using namespace umbra::night;

TEST_CASE("options.csv sets the game's pace and camera; bad rows are refused (T147)", "[night][options]")
{
    std::vector<umbra::core::Diagnostic> diagnostics;
    GameOptions options;
    REQUIRE(LoadOptions(std::filesystem::path(UMBRA_LASTCURTAIN_ASSETS) / "options.csv", options, diagnostics));
    CHECK(options.tapToContinue);
    CHECK(options.moments);
    CHECK(options.beatSeconds > GameOptions{}.beatSeconds); // slower than before (user)
    CHECK(options.cameraMinDistance < options.cameraDistance);
    const std::filesystem::path bad = std::filesystem::temp_directory_path() / "umbra-options.csv";
    for (const char *row : {"noSuchOption,1,x\n", "beatSeconds,fast,x\n", "beatSeconds,-1,x\n"})
    {
        std::ofstream(bad) << "key,value,note\n" << row;
        diagnostics.clear();
        GameOptions out;
        CHECK_FALSE(LoadOptions(bad, out, diagnostics));
        REQUIRE_FALSE(diagnostics.empty());
        CHECK(diagnostics.front().field.rfind("options.csv:1:", 0) == 0);
    }
    std::filesystem::remove(bad);
}
