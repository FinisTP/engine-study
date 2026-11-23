// UmbraTool talk-mind: the model's Vietnamese test (campaign plan 1, TalkMind.md). Runs a model file
// over the cafe test set and writes a report for the user; `compare` puts several runs side by side.

#include <ToolCommand.hpp>

#include <TalkMind.hpp>
#if UMBRA_TOWN_LLAMA
#include <LlamaMind.hpp>
#endif

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

namespace
{
    using namespace umbra::talk;
    namespace fs = std::filesystem;

    int Compare(const umbra::tool::Arguments &args)
    {
        std::vector<nlohmann::json> summaries;
        for (std::size_t i = 1; i < args.positional.size(); ++i)
        {
            std::ifstream in(args.positional[i]);
            nlohmann::json summary = nlohmann::json::parse(in, nullptr, false);
            if (!summary.is_object() || summary.value("format", "") != "umbra.lc-model-run")
            {
                std::cerr << args.positional[i] << ": not a talk-mind summary (.json)\n";
                return 1;
            }
            summaries.push_back(std::move(summary));
        }
        if (summaries.empty())
            return umbra::tool::Usage();
        if (!args.Has("out"))
        {
            WriteComparison(summaries, std::cout);
            return 0;
        }
        std::ofstream out(args.Path("out"));
        WriteComparison(summaries, out);
        std::cout << "wrote " << args.Path("out").generic_string() << "\n";
        return 0;
    }

    int TalkMindCommand(const umbra::tool::Arguments &args)
    {
        if (!args.positional.empty() && args.positional[0] == "compare")
            return Compare(args);
        if (!args.Has("model"))
            return umbra::tool::Usage();
#if UMBRA_TOWN_LLAMA
        // 1. The test set.
        std::vector<umbra::core::Diagnostic> diagnostics;
        TestSet set;
        if (!LoadTestSet(args.Has("data") ? args.Path("data") : fs::path(UMBRA_LASTCURTAIN_CAMPAIGN_DATA), set, diagnostics))
            return umbra::tool::Report(diagnostics, 1, args.json);
        const fs::path modelFile = args.Path("model");
        const fs::path outDir = args.Has("out") ? args.Path("out") : fs::path("talk-mind-reports");
        ModelRun run;
        run.model = modelFile.filename().string();
        const auto ms = [](auto from) { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - from).count(); };
        // 2. The model, loaded and warmed up as the game does it; the cards' free memory before and after.
        umbra::town::LlamaOptions options;
        options.threads = args.Int("threads", std::max(2, static_cast<int>(std::thread::hardware_concurrency()) / 2));
        options.gpuLayers = args.Int("gpu", -1);
        options.showPrompt = args.Has("show-prompt");
        const long long freeBefore = umbra::town::LlamaGpuFreeBytes();
        std::string error;
        auto start = std::chrono::steady_clock::now();
        std::unique_ptr<umbra::town::Mind> model = umbra::town::LoadLlamaMind(modelFile, options, error);
        run.loadMs = ms(start);
        if (!model)
            run.loadError = modelFile.generic_string() + ": " + error;
        else
        {
            start = std::chrono::steady_clock::now();
            model->WarmUp();
            run.warmMs = ms(start);
            const long long freeAfter = umbra::town::LlamaGpuFreeBytes();
            if (freeBefore >= 0 && freeAfter >= 0)
                run.videoMb = static_cast<int>((freeBefore - freeAfter) / (1024 * 1024));
            std::cout << "model " << run.model << ": loaded in " << run.loadMs << " ms, warm-up " << run.warmMs << " ms, graphics card: "
                      << (umbra::town::LlamaHasGpu() ? "yes" : "not in this build") << "\n";
            // 3. Every situation, every seed.
            RunTest(set, *model, static_cast<std::uint32_t>(args.Long("seed", 1)), run);
        }
        // 4. The report and its summary, named after the model file.
        fs::create_directories(outDir);
        const fs::path report = outDir / (modelFile.stem().string() + ".md");
        {
            std::ofstream out(report);
            WriteReport(set, run, out);
        }
        std::ofstream(outDir / (modelFile.stem().string() + ".json")) << Summary(set, run).dump(2) << "\n";
        const std::string verdict = Verdict(set, run);
        std::cout << verdict << "\nreport: " << report.generic_string() << "\n";
        if (!run.loadError.empty())
            return 1;
        return verdict == "passes the automatic checks" ? 0 : 2;
#else
        std::cerr << "This UmbraTool was built without llama.cpp (vcpkg feature town-model or town-model-gpu).\n";
        return 1;
#endif
    }
}

UMBRA_TOOL_COMMAND(TalkMind, "talk-mind",
                   "talk-mind --model <gguf file> [--data <campaign folder>] [--out <folder>] [--seed 1] [--threads n] [--gpu layers (default all, 0 = CPU)] [--show-prompt 1]\n"
                   "  talk-mind compare <summary.json>... [--out <file.md>]",
                   TalkMindCommand);
