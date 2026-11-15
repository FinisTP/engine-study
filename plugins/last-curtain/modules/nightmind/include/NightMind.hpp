#pragma once

#include <Mind.hpp>
#include <Narrate.hpp>
#include <NightDef.hpp>

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

// The model's lines (plan 3, NightMind.md): a request that asks the built-in model to reword one
// template line in a guest's voice, and the guard its answer must pass. Nothing here changes the night.
namespace umbra::night
{
    enum class LineKind
    {
        Caption,    // an action in the player's light, as narration
        Question,   // a chat question a guest asks the player
        GoalDone,   // a guest on their goal, done
        GoalFailed, // ... or failed
        Turn,       // midnight: the guest's last words, then the Taken's first
        Taken,      // the Taken's taunt as it acts
        Reply       // a guest answers the player's answer to its question: pleased or not
    };

    struct LineAsk
    {
        LineKind kind = LineKind::Caption;
        int speaker = 0;         // guest index
        bool taken = false;      // the speaker speaks as its Taken form
        std::vector<int> about;  // guests the line is about (pronouns in the prompt)
        std::string line;        // the template line to reword
        std::uint32_t seed = 0;  // night seed + beat number: the same night words the same
    };

    [[nodiscard]] umbra::town::MindRequest Request(const NightDef &def, const LineAsk &ask);

    // The tidied answer if it passes every check, else "" (show the template).
    [[nodiscard]] std::string Guard(const NightDef &def, const LineAsk &ask, const std::string &answer);

    // The guests a line names, by name or Taken name (for LineAsk::about).
    [[nodiscard]] std::vector<int> Named(const NightDef &def, const std::string &line);

    // UmbraTool night-mind: `nights` nights of bots; spread over each night, up to `perNight` of their
    // captions and chat questions, and every Turn, asked of `mind` and printed with the verdict and time.
    struct LineReport
    {
        int lines = 0, accepted = 0;
        long long totalMs = 0, slowestMs = 0;
    };
    LineReport TryLines(const NightDef &def, const Captions &captions, umbra::town::Mind &mind, int nights, std::uint64_t seed, int perNight, std::ostream &out);
}
