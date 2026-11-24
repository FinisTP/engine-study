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
    CHECK(options.boostCapPerStat == 3); // general drinks per stat per character (the user, campaign plan 6)
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
    // 4. Large text (campaign plan 5): off at first, a switch, saved, and its scale.
    std::filesystem::remove(file);
    AnalogScreen big = LoadAnalog(file, options);
    CHECK_FALSE(big.bigText);
    CHECK(big.TextScale() == 1.0F);
    CHECK(big.Toggle("bigtext", file));
    CHECK(LoadAnalog(file, options).bigText);
    CHECK(LoadAnalog(file, options).TextScale() == 1.25F);
    CHECK(big.Data()["bigText"] == true);
}

TEST_CASE("Settings (menus.md D): full screen, effects volume, text speed, enemy speed, language; saved and read back", "[night][options]")
{
    GameOptions options;
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "umbra-settings-test" / "screen.json";
    std::filesystem::remove_all(file.parent_path());
    AnalogScreen s = LoadAnalog(file, options);
    // 1. The defaults: a window, full volume, normal letters and beats, Vietnamese.
    CHECK_FALSE(s.fullScreen);
    CHECK(s.effects == 100);
    CHECK(s.textSpeed == 1);
    CHECK(s.LetterScale() == 1.0F);
    CHECK(s.EnemyScale() == 1.0F);
    CHECK_FALSE(s.english);
    // 2. Each action changes one thing; volume moves by 10 and stays in 0..100; speeds cycle.
    CHECK(s.Toggle("full", file));
    CHECK(s.Toggle("fx-", file));
    CHECK(s.effects == 90);
    for (int i = 0; i < 12; ++i)
        (void)s.Toggle("fx+", file);
    CHECK(s.effects == 100);
    CHECK(s.Toggle("textspeed", file));
    CHECK(s.textSpeed == 2);
    CHECK(s.LetterScale() == 2.0F);
    (void)s.Toggle("textspeed", file);
    CHECK(s.LetterScale() > 100.0F); // Ngay: the whole line at once
    (void)s.Toggle("textspeed", file);
    CHECK(s.textSpeed == 0);
    CHECK(s.Toggle("enemyspeed", file));
    CHECK(s.EnemyScale() == 2.0F);
    CHECK(s.music == 70);           // music starts a little under the effects
    CHECK(s.Toggle("mu-", file));
    CHECK(s.music == 60);
    CHECK(s.Toggle("language", file));
    CHECK_FALSE(s.reducedFx); // visual effects start full (VFX library)
    CHECK(s.Toggle("vfx", file));
    CHECK(s.reducedFx);
    // 3. All of it read back after a restart; the screen's data names each state.
    const AnalogScreen again = LoadAnalog(file, options);
    CHECK(again.fullScreen);
    CHECK(again.textSpeed == 0);
    CHECK(again.EnemyScale() == 2.0F);
    CHECK(again.english);
    CHECK(again.Data()["full"] == true);
    CHECK(again.Data()["effects"] == 100);
    CHECK(again.Data()["textSpeed0"] == true);
    CHECK(again.Data()["textSpeed1"] == false);
    CHECK(again.Data()["enemyFast"] == true);
    CHECK(again.Data()["music"] == 60);
    CHECK(again.Data()["english"] == true);
    CHECK(again.reducedFx);
    CHECK(again.Data()["vfxReduced"] == true);
    CHECK(again.Data()["vfxFull"] == false);
}
