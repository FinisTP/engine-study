#pragma once

#include <AssetFolders.hpp>
#include <Diagnostic.hpp>
#include <SoundAssistant.hpp>

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace umbra::localai
{
    // Whole generations through a local ComfyUI: idea -> new asset files with provenance. Used by
    // UmbraTool (texture, sound, model) and the agent's generate_* tools (S5). Blocking: run them
    // on a worker thread. See LocalAi.md.
    struct GenerateOptions final
    {
        std::string idea;              // the prompt
        std::string name;              // file name base ("" = from the idea)
        assets::AssetFolders assets;   // read the styles here; new files go into assets.Project()
        std::string host;              // "" = find ComfyUI on port 8000 or 8188
        std::string model;             // checkpoint ("" = the preferred installed one)
        int count = 1;                 // variations (texture, sound)
        long long seed = -1;           // -1 = random
        // Texture
        float meters = 2.0F;
        std::uint32_t size = 1024;
        // Sound
        SoundKind kind = SoundKind::Music;
        float seconds = 0.0F;          // 0 = 60 s music, 2 s effect
        int bpm = 0;                   // 0 = the request's default
        std::string key;
        // Model: a picture file or a ComfyUI job id instead of the idea; the exported workflow
        std::filesystem::path image;
        std::string resume;
        std::filesystem::path workflow; // "" = the first exported workflow found
        int faces = 10000;
        std::function<void(const std::string &job)> queued; // model: ComfyUI's job id (worker thread)
        // Rig / motion (S8): the model to rig, or the character a motion is made for ("" = none)
        std::string source;

        std::function<void(const std::string &what, std::size_t done, std::size_t total, float seconds)> progress; // ~4 times a second
        const std::atomic<bool> *stop = nullptr; // set to cancel
    };

    struct Made final
    {
        std::string path;     // asset path: a material, a sound or a model
        long long seed = 0;
        nlohmann::json info;  // seam, seconds, triangles...
    };

    struct GenerateResult final
    {
        std::string host, model;
        std::vector<Made> made;   // empty = nothing was made (see diagnostics)
        std::vector<core::Diagnostic> diagnostics;
    };

    [[nodiscard]] GenerateResult CreateTexture(const GenerateOptions &options);
    [[nodiscard]] GenerateResult CreateSound(const GenerateOptions &options);
    [[nodiscard]] GenerateResult CreateModel(const GenerateOptions &options);
    // S8: a model -> the exported rigging workflow -> models/<name>.glb with a skeleton.
    [[nodiscard]] GenerateResult CreateRig(const GenerateOptions &options);
    // S8: idea (what the character does) -> the exported text-to-motion workflow -> animations/<name>.glb.
    [[nodiscard]] GenerateResult CreateMotion(const GenerateOptions &options);
}
