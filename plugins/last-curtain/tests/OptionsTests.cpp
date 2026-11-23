#include "NightTestData.hpp"

#include <Analog.hpp>
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

TEST_CASE("The analog screen: strengths from options.csv, the player's two switches saved and applied (S45)", "[night][options]")
{
    // 1. The designer's strengths; both switches start on (the user, 2026-11-23).
    std::vector<umbra::core::Diagnostic> diagnostics;
    GameOptions options;
    REQUIRE(LoadOptions(std::filesystem::path(UMBRA_LASTCURTAIN_ASSETS) / "options.csv", options, diagnostics));
    CHECK(options.crt > 0.0F);
    CHECK(options.vhs > 0.0F);
    CHECK(options.lettersPerSecond > 0.0F);
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "umbra-analog-test" / "screen.json";
    std::filesystem::remove_all(file.parent_path());
    AnalogScreen analog = LoadAnalog(file, options); // no file yet: on
    CHECK(analog.crtOn);
    CHECK(analog.vhsOn);
    umbra::core::PostSettings post;
    analog.Apply(post);
    CHECK(post.crt == options.crt);
    CHECK(post.vhs == options.vhs);
    // 2. A switch from the pause screen turns one off, is saved at once, and survives a restart.
    CHECK(analog.Toggle("vhs", file));
    CHECK_FALSE(analog.Toggle("save", file)); // not ours
    analog.Apply(post);
    CHECK(post.vhs == 0.0F);
    CHECK(post.crt == options.crt);
    const AnalogScreen again = LoadAnalog(file, options);
    CHECK(again.crtOn);
    CHECK_FALSE(again.vhsOn);
    CHECK(again.Data()["vhsOff"] == true);
    CHECK(again.Data()["crt"] == true);
    // 3. A broken file keeps the defaults; no folder (no preferences) still plays, unsaved.
    std::ofstream(file) << "{ not json";
    CHECK(LoadAnalog(file, options).vhsOn);
    AnalogScreen nowhere = LoadAnalog({}, options);
    CHECK(nowhere.Toggle("crt", {}));
    CHECK_FALSE(nowhere.crtOn);
}
