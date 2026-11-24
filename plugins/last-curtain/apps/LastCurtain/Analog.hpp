#pragma once

#include "Options.hpp"

#include <Post.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>

// The analog screen (CRT and VHS, engine/core/include/Post.md "6"): the designer sets how strong
// each is (options.csv crt, vhs); the player turns each on or off (saved). With them, the player's
// other settings (menus.md D): full screen, effects volume, text and enemy speed, language.
namespace umbra::night
{
    struct AnalogScreen
    {
        float crt = 0.0F, vhs = 0.0F;    // strengths from options.csv
        bool crtOn = true, vhsOn = true; // the player's switches (on until turned off, the user)
        bool bigText = false;            // large text (campaign plan 5): fonts x1.25
        bool fullScreen = false, enemyFast = false, english = false;
        int effects = 100;  // sound effects volume, 0..100 by 10
        int music = 70;     // music volume, the same way
        int textSpeed = 1;  // 0 slow, 1 normal, 2 fast, 3 at once
        [[nodiscard]] float TextScale() const { return bigText ? 1.25F : 1.0F; }
        [[nodiscard]] float LetterScale() const { return textSpeed == 0 ? 0.5F : textSpeed == 2 ? 2.0F : textSpeed == 3 ? 1000.0F : 1.0F; }
        [[nodiscard]] float EnemyScale() const { return enemyFast ? 2.0F : 1.0F; } // the enemy's beats run this much faster

        void Apply(core::PostSettings &post) const;
        // A settings action (crt, vhs, bigtext, full, fx-, fx+, textspeed, enemyspeed, language): changes it and
        // saves to `file` ("" = don't save). False for other actions.
        bool Toggle(const std::string &action, const std::filesystem::path &file);
        // For the settings screen: each switch and its Off, effects, textSpeed0..3, enemyFast/enemyNormal, english/vietnamese.
        [[nodiscard]] nlohmann::json Data() const;
    };

    // <preferences>/last-curtain/screen.json ("" when the system gives no folder).
    [[nodiscard]] std::filesystem::path AnalogPath();
    // The strengths from `options`; the switches from `file` (missing or broken: both on).
    [[nodiscard]] AnalogScreen LoadAnalog(const std::filesystem::path &file, const GameOptions &options);
}
