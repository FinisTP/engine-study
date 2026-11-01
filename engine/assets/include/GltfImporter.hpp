#pragma once

#include <Model.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace umbra::assets
{
    struct ImportResult final
    {
        Model model;
        std::vector<std::string> dependencies; // external files the source reads, relative to its folder
        std::vector<std::string> warnings;     // unsupported features that were skipped
        std::vector<std::string> errors;       // any error means `model` must not be used

        [[nodiscard]] bool Ok() const noexcept { return errors.empty(); }
    };

    // Reads a .gltf or .glb file into a Model. Never throws; problems are listed in the result.
    [[nodiscard]] ImportResult ImportGltf(const std::filesystem::path &path);
}
