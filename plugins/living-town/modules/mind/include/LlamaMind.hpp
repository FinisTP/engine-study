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
        int gpuLayers = 0;     // layers on the GPU (0 = CPU only; 99 = all, if llama.cpp was built for a GPU)
        int context = 1024;    // tokens the model sees at once: a prompt here is about 150
        float temperature = 0.7F;
    };

    // Loads the model; null with `error` set if the file is missing or not a usable GGUF.
    [[nodiscard]] std::unique_ptr<Mind> LoadLlamaMind(const std::filesystem::path &file, const LlamaOptions &options, std::string &error);
}
