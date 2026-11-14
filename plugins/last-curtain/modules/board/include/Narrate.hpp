#pragma once

#include <Night.hpp>

#include <Diagnostic.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

// One action told as the player may see it: where to look, the pawn's walk and two lines of text (Board.md).
namespace umbra::night
{
    struct Beat
    {
        int guest = -1;
        int camera = -1;        // the cell to look at; -1 = nothing seen
        std::vector<int> path;  // cells the pawn walks, seen part only
        std::string caption;    // the decision; "" = nothing to say
        std::string result;     // what came of it, from the log
        bool seen = false;
    };

    // captions.json: key -> line with {guest}, {other}, {room}, {foe}, {omen}, {power}, {item} ... filled in.
    struct Captions
    {
        std::map<std::string, std::string> lines;
    };

    // Every key the narrator uses must be there. Diagnostic code "night.captions".
    bool LoadCaptions(const std::filesystem::path &file, Captions &out, std::vector<core::Diagnostic> &diagnostics);

    // `entries` are the log entries `action` wrote; `actor` the guest who took it.
    Beat Narrate(const NightDef &def, const NightState &before, const NightState &after,
                 const std::vector<LogEntry> &entries, const Action &action, int actor, int viewer,
                 const Captions &captions);
}
