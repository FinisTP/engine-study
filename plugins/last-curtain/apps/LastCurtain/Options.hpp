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
        float blowSeconds = 1.8F;        // a blow in a fight, told in words, before the next (a click goes on)
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
        float turnBannerSeconds = 0.0F;  // each turn opens with its banner this long (the bot waits)
        float plainBeatSeconds = 1.5F;   // a seen bot step where nothing happens (a plain room)
        float tapGap = 0.0F;             // taps closer than this are ignored (spam clicks)
        bool light = true;               // false: everyone sees everything (the user's test switch)
        float matchCameraDistance = 26.0F; // the faction match's table: the seat's distance
        float matchCameraTilt = 58.0F;     // ... and its degrees above the table
        float orderLength = 12.0F;         // portraits in the match's order strip
        float crt = 0.0F;                  // the analog screen's CRT strength, 0..1 (Analog.hpp)
        float vhs = 0.0F;                  // ... and its VHS tape strength, 0..1
        float boostCapPerStat = 3.0F;      // general drinks per stat per character (campaign plan 6)
        float lettersPerSecond = 40.0F;    // a dialog line appears this fast (DialogApp.hpp)
        float stageTilesAcross = 0.0F;     // a campaign stage's seat shows this many tiles across (0: matchCameraDistance)
        float turnPause = 0.0F;            // a bot waits this long when a new turn begins on the board (note 18)
        float battleTurnPause = 0.0F;      // ... and when a new pick begins in a battle
    };

    // Reads key,value,note rows; an unknown key or a bad value is refused with its row.
    bool LoadOptions(const std::filesystem::path &file, GameOptions &options, std::vector<core::Diagnostic> &diagnostics);
}
