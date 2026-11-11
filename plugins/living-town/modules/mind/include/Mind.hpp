#pragma once

#include <cstdint>
#include <string>

// A language model as the town sees it (S23): text in, text out, nothing else. The real one is
// LlamaMind (llama.cpp); tests use a scripted one. See Mind.md.
namespace umbra::town
{
    struct MindRequest final
    {
        std::string prompt;
        std::string grammar;   // GBNF: the answer can only be text this grammar allows
        int maxTokens = 48;
        std::uint32_t seed = 0;
    };

    class Mind
    {
    public:
        virtual ~Mind() = default;
        [[nodiscard]] virtual std::string Name() const = 0;
        // Answers one request. Called on the town's worker thread, never on the frame.
        // An empty answer means it failed.
        virtual std::string Complete(const MindRequest &request) = 0;
    };
}
