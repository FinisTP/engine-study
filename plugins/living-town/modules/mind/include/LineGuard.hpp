#pragma once

#include <Town.hpp>

#include <filesystem>
#include <string>
#include <vector>

// What a model-written line must pass before a player sees it (S23, Mind.md "The guard").
// A list of small checks; the first that objects wins and the template line is used instead.
namespace umbra::town
{
    struct LineContext final
    {
        const TownDef &def;
        const Fact &fact;  // what the line is about
        int speaker = 0;
        std::string templated = {}; // the template line it rewords (for checks that compare with it)
    };

    struct LineRules final
    {
        int maxLength = 160;
        std::vector<std::string> banned; // words or phrases, compared without case
    };
    // Reads `town/lines.rules.json` ({"maxLength": 160, "banned": ["..."]}); a missing file = defaults.
    bool LoadLineRules(const std::filesystem::path &file, LineRules &rules, std::vector<core::Diagnostic> &diagnostics);

    // One check: empty = fine, else the reason it objects (for the log and the counters).
    using LineFilter = std::string (*)(const LineContext &context, const LineRules &rules, const std::string &line);

    struct LineGuard final
    {
        LineRules rules;
        std::vector<LineFilter> filters; // run in order; add your own with push_back

        LineGuard(); // the built-in checks: clean text, length, banned words, only the fact's people, all of them, no shouting, same yes-or-no, no false witness
        [[nodiscard]] std::string Objection(const LineContext &context, const std::string &line) const;
    };

    // Trims spaces and wrapping quotes a model likes to add.
    [[nodiscard]] std::string Tidy(std::string line);
}
