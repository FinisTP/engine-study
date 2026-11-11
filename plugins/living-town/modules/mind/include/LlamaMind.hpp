#pragma once

#include "Mind.hpp"

#include <filesystem>
#include <memory>

// The real model: a GGUF file run by llama.cpp on the CPU or GPU (Mind.md "The model").
// Only built when the build found llama.cpp (UMBRA_TOWN_LLAMA).
namespace umbra::town
{
    struct LlamaOptions final
    {
        int threads = 4;       // CPU threads for one answer
        int gpuLayers = -1;    // layers on the graphics card: -1 = all if this build can use one, 0 = CPU only, n = that many
        int context = 1024;    // tokens the model sees at once: a prompt here is about 150
        float temperature = 0.6F;
        bool showPrompt = false; // print the first prompt exactly as the model gets it (for tuning)
    };

    // Loads the model; null with `error` set if the file is missing or not a usable GGUF.
    // Whether this build of llama.cpp can run on a graphics card at all.
    [[nodiscard]] bool LlamaHasGpu();
    [[nodiscard]] std::unique_ptr<Mind> LoadLlamaMind(const std::filesystem::path &file, const LlamaOptions &options, std::string &error);
}
