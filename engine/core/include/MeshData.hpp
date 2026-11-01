#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace umbra::core
{
    // CPU-side asset data shared by the importer (Assets) and the uploader (Render).

    struct Vertex final
    {
        glm::vec3 position{};
        glm::vec3 normal{};
        glm::vec2 uv{};
    };

    // Indexed triangle list with one material color.
    struct MeshData final
    {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
        glm::vec4 baseColor{1.0F};
    };

    // Tightly packed 8-bit RGBA pixels, row by row.
    struct TextureData final
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<std::uint8_t> rgba;
    };

    // Return an empty string when valid, otherwise a short reason.
    [[nodiscard]] std::string ValidateMesh(const MeshData &mesh);
    [[nodiscard]] std::string ValidateTexture(const TextureData &texture);
}
