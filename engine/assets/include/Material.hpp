#pragma once

#include <Diagnostic.hpp>

#include <glm/vec4.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace umbra::assets
{
    // How an AI-generated texture was made: enough to make it again and to check its license.
    struct Generated final
    {
        std::string provider; // "comfyui"
        std::string model;    // checkpoint file name
        std::string license;  // filled from ModelLicense when generated
        std::string prompt;
        std::string negative;
        long long seed = 0;
        int steps = 0;
        std::string date;
    };

    // A surface look for scene objects: "materials/<name>.material.json" (Material.md).
    struct Material final
    {
        std::string baseColor;        // image path relative to the asset folder
        bool worldProjection = false; // true: repeat by meters in world space; false: the model's UVs
        float metersPerRepeat = 2.0F;
        glm::vec4 tint{1.0F};
        std::optional<Generated> generated;
    };

    inline constexpr int materialFormatVersion = 1;

    // Parses and checks the fields. The image itself is checked by CheckMaterialImage.
    bool ParseMaterial(const std::string &text, const std::string &name, Material &material, std::vector<core::Diagnostic> &diagnostics);
    bool LoadMaterial(const std::filesystem::path &file, Material &material, std::vector<core::Diagnostic> &diagnostics);
    // The image exists under `assetRoot`, is readable, and has power-of-two sides from 64 to 2048.
    bool CheckMaterialImage(const std::filesystem::path &assetRoot, const Material &material, const std::string &name,
                            std::vector<core::Diagnostic> &diagnostics);
    // The file text for a material (stable key order, two-space indent).
    [[nodiscard]] std::string MaterialText(const Material &material);

    // What is known about an image model's license, from its checkpoint file name.
    struct ModelLicenseInfo final
    {
        std::string name;        // e.g. "CreativeML OpenRAIL++-M"
        bool commercial = false; // outputs may be used in a sold game
        bool known = false;
    };
    [[nodiscard]] ModelLicenseInfo ModelLicense(const std::string &checkpoint);
    // A warning (`material.license`) for unknown or non-commercial licenses; nothing otherwise.
    void CheckMaterialLicense(const Material &material, const std::string &name, std::vector<core::Diagnostic> &diagnostics);
}
