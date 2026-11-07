#pragma once

#include <ComfyUi.hpp>
#include <Material.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace umbra::localai
{
    // Glue between ComfyUI and materials, shared by the editor and UmbraTool (roadmap P8C.3).

    // The project's look, added to every prompt: assets/materials/style.json {"prompt", "negative"}.
    struct TextureStyle final
    {
        std::string prompt = "seamless tileable texture, flat even lighting, top-down view, stylized hand-painted game art";
        std::string negative = "text, watermark, signature, frame, border, perspective, shadows, objects, people, blurry";
    };
    [[nodiscard]] TextureStyle LoadTextureStyle(const std::filesystem::path &assetRoot);

    // An SDXL checkpoint if one is installed (the decided default), else the first; "" if none.
    [[nodiscard]] std::string PreferredCheckpoint(const std::vector<std::string> &installed, const std::string &wanted = "");

    // One request per seed: "<idea>, <style prompt>".
    [[nodiscard]] std::vector<ImageRequest> MakeTextureRequests(const std::string &idea, const TextureStyle &style, const std::string &model,
                                                                const std::string &host, const std::vector<long long> &seeds);
    // Seeds for `count` variations (random unless `first` is given: first, first+1, ...).
    [[nodiscard]] std::vector<long long> TextureSeeds(int count, long long first = -1);

    // A generated picture made ready for a material: power-of-two size, seamless, scored.
    struct TextureCandidate final
    {
        core::TextureData image;
        float seamBefore = 0.0F; // seam score of the raw picture
        float seam = 0.0F;       // after MakeSeamless; above assets::visibleSeam = warn
        ImageRequest request;    // how it was made
    };
    [[nodiscard]] TextureCandidate FinishTexture(const core::TextureData &raw, const ImageRequest &request, std::uint32_t size = 1024);

    // Writes textures/<name>.png and materials/<name>.material.json (new files; "_2", "_3"... if
    // taken), each through a temporary file. Returns the material path relative to the asset
    // folder, or "" with diagnostics.
    [[nodiscard]] std::string SaveTextureMaterial(const std::filesystem::path &assetRoot, const std::string &name, const TextureCandidate &candidate,
                                                  float metersPerRepeat, std::vector<core::Diagnostic> &diagnostics);
    // "Mossy cobblestone!" -> "mossy_cobblestone" (letters, digits, '_'; at most 32).
    [[nodiscard]] std::string TextureName(const std::string &text);
}
