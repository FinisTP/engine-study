// UmbraTool match-check, match-soak: the faction match without graphics (Bots.md "The soak").

#include <ToolCommand.hpp>

#include <MatchSoak.hpp>

#include <chrono>
#include <iostream>

namespace
{
    using namespace umbra::match;

    std::filesystem::path DataFolder(const umbra::tool::Arguments &args)
    {
        return args.Has("data") ? args.Path("data") : std::filesystem::path(UMBRA_MATCH_DEFAULT_DATA);
    }

    BotLevel Level(const std::string &word)
    {
        return word == "easy" ? BotLevel::Easy : word == "hard" ? BotLevel::Hard : BotLevel::Medium;
    }

    int MatchCheck(const umbra::tool::Arguments &args)
    {
        std::vector<umbra::core::Diagnostic> diagnostics;
        MatchDef def;
        LoadMatch(DataFolder(args), def, diagnostics);
        return umbra::tool::Report(diagnostics, 11, args.json);
    }

    int MatchSoak(const umbra::tool::Arguments &args)
    {
        std::vector<umbra::core::Diagnostic> diagnostics;
        MatchDef def;
        if (!LoadMatch(DataFolder(args), def, diagnostics))
            return umbra::tool::Report(diagnostics, 11, args.json);
        // 1. --levels side0,side1 (easy, medium, hard).
        const std::string levels = args.Text("levels", "medium,medium");
        const auto comma = levels.find(',');
        const BotLevel a = Level(levels.substr(0, comma)), b = Level(comma == std::string::npos ? levels : levels.substr(comma + 1));
        const auto start = std::chrono::steady_clock::now();
        const MatchReport r = SoakMatches(def, static_cast<std::uint64_t>(args.Long("seed", 1)), args.Int("runs", 200), a, b);
        std::cout << Text(def, r) << (r.Holds(def) ? "the data holds" : "THE DATA DOES NOT HOLD") << " ("
                  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << " ms)\n";
        return r.Holds(def) ? 0 : 1;
    }
}

UMBRA_TOOL_COMMAND(MatchCheck, "match-check", "match-check [--data <folder>] [--json]", MatchCheck);
UMBRA_TOOL_COMMAND(MatchSoak, "match-soak", "match-soak [--data <folder>] [--runs 200] [--seed 1] [--levels medium,medium]", MatchSoak);
