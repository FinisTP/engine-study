// UmbraTool night-mind: tries a model file on a few bot nights of The Last Curtain (plan 3): each
// line's template, the model's rewording, the guard's verdict and how long it took.

#include <ToolCommand.hpp>

#include <NightMind.hpp>
#if UMBRA_TOWN_LLAMA
#include <LlamaMind.hpp>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

namespace
{
    using namespace umbra::night;

    int NightMindCommand(const umbra::tool::Arguments &args)
    {
        if (!args.Has("model"))
            return umbra::tool::Usage();
#if UMBRA_TOWN_LLAMA
        // 1. The night's data and captions (the plugin's own, or --data).
        const std::filesystem::path data = args.Has("data") ? args.Path("data") : std::filesystem::path(UMBRA_NIGHT_DEFAULT_DATA);
        std::vector<umbra::core::Diagnostic> diagnostics;
        NightDef def;
        Captions captions;
        if (!LoadNight(data, def, diagnostics) || !LoadCaptions(data / "captions.json", captions, diagnostics))
            return umbra::tool::Report(diagnostics, 1, args.json);
        // 2. The model, warmed up as the game does it.
        umbra::town::LlamaOptions options;
        options.threads = args.Int("threads", std::max(2, static_cast<int>(std::thread::hardware_concurrency()) / 2));
        options.gpuLayers = args.Int("gpu", -1);
        options.showPrompt = args.Has("show-prompt");
        std::string error;
        std::unique_ptr<umbra::town::Mind> model = umbra::town::LoadLlamaMind(args.Path("model"), options, error);
        if (!model)
        {
            std::cerr << error << "\n";
            return 1;
        }
        std::cout << "model " << model->Name() << ", graphics card: " << (umbra::town::LlamaHasGpu() ? "yes" : "not in this build") << "\n";
        model->WarmUp();
        // 3. The nights.
        const LineReport report = TryLines(def, captions, *model, args.Int("nights", 3), static_cast<std::uint64_t>(args.Long("seed", 1)), args.Int("lines", 10), std::cout);
        return report.lines > 0 ? 0 : 1;
#else
        std::cerr << "This UmbraTool was built without llama.cpp (vcpkg feature town-model or town-model-gpu).\n";
        return 1;
#endif
    }
}

UMBRA_TOOL_COMMAND(NightMind, "night-mind", "night-mind --model <gguf file> [--data <folder>] [--nights 3] [--seed 1] [--lines 10] [--threads n] [--gpu layers (default all, 0 = CPU)] [--show-prompt 1]", NightMindCommand);
