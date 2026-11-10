#pragma once

#include <Diagnostic.hpp>

#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace umbra::authoring
{
    // What a model file holds, measured by the game's own importer.
    struct ModelInfo final
    {
        std::size_t triangles = 0;
        std::size_t meshes = 0;
        std::uint32_t textureSize = 0; // largest side of its biggest texture
        glm::vec3 min{0.0F};
        glm::vec3 max{0.0F};
        std::size_t joints = 0;          // 0 = not rigged (S8)
        std::vector<std::string> clips;  // animation clip names
    };
    bool InspectModel(const std::filesystem::path &file, ModelInfo &info, std::vector<core::Diagnostic> &diagnostics);

    // A static box collider fitted to the model's bounds at this entity scale (sizes in meters).
    [[nodiscard]] nlohmann::ordered_json FittedCollider(const ModelInfo &info, glm::vec3 scale);
}
