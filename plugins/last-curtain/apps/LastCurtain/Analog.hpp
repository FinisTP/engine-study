#pragma once

#include "Options.hpp"

#include <Post.hpp>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>

// The analog screen (CRT and VHS, engine/core/include/Post.md "6"): the designer sets how strong
// each is (options.csv crt, vhs); the player turns each on or off in the pause menu (saved).
namespace umbra::night
{
    struct AnalogScreen
    {
        float crt = 0.0F, vhs = 0.0F;    // strengths from options.csv
        bool crtOn = true, vhsOn = true; // the player's switches (on until turned off, the user)

        void Apply(core::PostSettings &post) const;
        // "crt" or "vhs" from the pause screen: flips it and saves to `file` ("" = don't save). False for other actions.
        bool Toggle(const std::string &action, const std::filesystem::path &file);
        // For the pause screen: crt, crtOff, vhs, vhsOff.
        [[nodiscard]] nlohmann::json Data() const;
    };

    // <preferences>/last-curtain/screen.json ("" when the system gives no folder).
    [[nodiscard]] std::filesystem::path AnalogPath();
    // The strengths from `options`; the switches from `file` (missing or broken: both on).
    [[nodiscard]] AnalogScreen LoadAnalog(const std::filesystem::path &file, const GameOptions &options);
}
