#pragma once

#include <Diagnostic.hpp>

#include <filesystem>
#include <vector>

// The game's pace and camera, for the designer to tune without a rebuild (assets/options.csv;
// user 2026-11-15). The defaults here are plan 2c's timings, which the tests use.
namespace umbra::night
{
    struct GameOptions
    {
        float slideSeconds = 0.25F;      // one step of a pawn
        float thinkSeconds = 0.4F;       // the shortest "thinking..." of a bot in view
        float lineSeconds = 3.0F;        // the longest wait for the model's line
        float beatSeconds = 1.5F;        // a seen bot action stays on screen
        float fastBeatSeconds = 0.3F;    // ... after Space
        float unseenBeatSeconds = 0.2F;  // an action out of sight
        float diceSeconds = 0.0F;        // a die tumbles this long before it lands
        bool tapToContinue = false;      // a beat where something happens waits for a tap
        bool moments = false;            // ... and is shown at full size on the stage
        float cameraDistance = 11.0F;    // metres from the table
        float cameraMinDistance = 5.0F;  // the wheel's closest
        float cameraMaxDistance = 22.0F; // the wheel's farthest
        float cameraTilt = 50.0F;        // degrees above the table
        float cameraEaseSeconds = 0.4F;  // the camera's glide to a new place
    };

    // Reads key,value,note rows; an unknown key or a bad value is refused with its row.
    bool LoadOptions(const std::filesystem::path &file, GameOptions &options, std::vector<core::Diagnostic> &diagnostics);
}
