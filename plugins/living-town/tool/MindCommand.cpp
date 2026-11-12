// UmbraTool town-mind: tries a model file on a town and measures it (S23): how each resident words
// their news, what the guard makes of it, which activities it picks, and how long an answer takes.

#include <ToolCommand.hpp>

#include <TownMind.hpp>
#if UMBRA_TOWN_LLAMA
#include <LlamaMind.hpp>
#endif

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace
{
    using namespace umbra::town;
    using umbra::tool::Arguments;

    int TownMindCommand(const Arguments &args)
    {
        if (args.positional.size() != 1 || !args.Has("model"))
            return umbra::tool::Usage();
#if UMBRA_TOWN_LLAMA
        std::vector<umbra::core::Diagnostic> diagnostics;
        TownDef def;
        if (!LoadTown(args.positional[0], nullptr, def, diagnostics))
            return umbra::tool::Report(diagnostics, 1, args.json);
        LlamaOptions options;
        options.threads = args.Int("threads", std::max(2, static_cast<int>(std::thread::hardware_concurrency()) / 2));
        options.gpuLayers = args.Int("gpu", -1);
        options.showPrompt = args.Has("show-prompt");
        std::string error;
        const auto loadStart = std::chrono::steady_clock::now();
        std::unique_ptr<Mind> model = LoadLlamaMind(args.Path("model"), options, error);
        if (!model)
        {
            std::cerr << error << "\n";
            return 1;
        }
        const auto ms = [](auto from) { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - from).count(); };
        std::cout << "model " << model->Name() << " loaded in " << ms(loadStart) << " ms, " << options.threads << " threads, graphics card: "
                  << (!LlamaHasGpu() ? "not in this build (CPU only)" : options.gpuLayers < 0 ? "all layers" : std::to_string(options.gpuLayers) + " layers") << "\n";
        const auto warmStart = std::chrono::steady_clock::now();
        model->WarmUp();
        std::cout << "warm-up (the game does this in the background): " << ms(warmStart) << " ms\n\n";

        // 1. A town some days in, so people have news.
        TownState state = NewTown(def, static_cast<std::uint64_t>(args.Long("seed", 1)));
        Run(def, state, args.Int("days", 9) * ticksPerDay);
        LineGuard guard;
        LoadLineRules(std::filesystem::path(args.positional[0]).parent_path() / "lines.rules.json", guard.rules, diagnostics);

        // 2. Every resident words everything they have to tell.
        long long lineMs = 0, slowest = 0;
        int lines = 0, passed = 0;
        for (int resident = 0; resident < static_cast<int>(def.residents.size()); ++resident)
            for (int said = 0; said < args.Int("lines", 3); ++said)
            {
                const int news = NextNews(def, state, resident);
                if (news < 0)
                    break;
                const Fact fact = state.facts[static_cast<std::size_t>(news)];
                const auto start = std::chrono::steady_clock::now();
                const std::string line = Tidy(model->Complete(WordingRequest(def, state, fact, resident)));
                const long long took = ms(start);
                const std::string objection = line.empty() ? "no answer" : guard.Objection({def, fact, resident, Says(def, fact, resident)}, line);
                std::cout << def.residents[static_cast<std::size_t>(resident)].name << "\n  template: " << Says(def, fact, resident) << "\n  model:    " << line
                          << "\n  " << (objection.empty() ? "accepted" : "REFUSED (" + objection + ")") << ", " << took << " ms\n";
                ++lines, passed += objection.empty(), lineMs += took, slowest = std::max(slowest, took);
                (void)TalkTo(def, state, resident, objection.empty() ? line : std::string());
            }

        // 3. Every resident picks what to do, as if free now.
        long long pickMs = 0;
        int picks = 0, valid = 0;
        std::cout << "\n";
        for (int resident = 0; resident < static_cast<int>(def.residents.size()); ++resident)
        {
            TownState freed = state;
            freed.residents[static_cast<std::size_t>(resident)].until = freed.tick;
            const std::vector<Intent> legal = LegalIntents(def, freed, resident);
            const auto start = std::chrono::steady_clock::now();
            const std::string answer = model->Complete(ChoiceRequest(def, freed, resident, legal));
            const long long took = ms(start);
            const int number = std::atoi(answer.c_str());
            const bool ok = number >= 1 && static_cast<std::size_t>(number) <= legal.size();
            std::cout << def.residents[static_cast<std::size_t>(resident)].name << " (" << legal.size() << " choices): "
                      << (ok ? def.activities[static_cast<std::size_t>(legal[static_cast<std::size_t>(number - 1)].activity)].name : "INVALID \"" + answer + "\"") << ", " << took << " ms\n";
            ++picks, valid += ok, pickMs += took;
        }
        std::cout << "\nlines: " << passed << " of " << lines << " accepted, " << (lines ? lineMs / lines : 0) << " ms each (slowest " << slowest << " ms)\n"
                  << "choices: " << valid << " of " << picks << " valid, " << (picks ? pickMs / picks : 0) << " ms each\n";
        return 0;
#else
        std::cerr << "This build has no llama.cpp (the vcpkg package \"llama-cpp\"): town-mind cannot load a model.\n";
        return 1;
#endif
    }
}

UMBRA_TOOL_COMMAND(TownMindCommand, "town-mind", "town-mind <town file> --model <gguf file> [--days 9] [--seed 1] [--lines 3] [--threads n] [--gpu layers (default all, 0 = CPU)] [--show-prompt 1]", TownMindCommand);
