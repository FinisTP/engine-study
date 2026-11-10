#include <MeshData.hpp>

#include <algorithm>

#include <cmath>

namespace umbra::core
{
    std::string ValidateMesh(const MeshData &mesh)
    {
        if (mesh.vertices.empty() || mesh.indices.empty())
        {
            return "mesh has no vertices or no indices";
        }
        if (mesh.indices.size() % 3 != 0)
        {
            return "index count is not a multiple of 3";
        }
        for (const std::uint32_t index : mesh.indices)
        {
            if (index >= mesh.vertices.size())
            {
                return "index " + std::to_string(index) + " is out of range";
            }
        }
        if (!mesh.skin.empty() && mesh.skin.size() != mesh.vertices.size())
        {
            return "skin data does not match the vertex count";
        }
        for (const Vertex &v : mesh.vertices)
        {
            if (!std::isfinite(v.position.x) || !std::isfinite(v.position.y) || !std::isfinite(v.position.z))
            {
                return "vertex position is not finite";
            }
        }
        return {};
    }

    std::string ValidateTexture(const TextureData &texture)
    {
        // 16384 is a common GPU limit and keeps a bad file from allocating gigabytes.
        constexpr std::uint32_t maxSize = 16384;
        if (texture.width == 0 || texture.height == 0 || texture.width > maxSize || texture.height > maxSize)
        {
            return "texture size is zero or larger than 16384";
        }
        if (texture.rgba.size() != std::size_t{texture.width} * texture.height * 4)
        {
            return "pixel data does not match width * height * 4";
        }
        return {};
    }

    std::vector<TextureData> MipChain(const TextureData &texture)
    {
        std::vector<TextureData> levels{texture};
        while (levels.back().width > 1 || levels.back().height > 1)
        {
            const TextureData &big = levels.back();
            TextureData small{std::max(1U, big.width / 2), std::max(1U, big.height / 2), {}};
            small.rgba.resize(std::size_t{small.width} * small.height * 4);
            for (std::uint32_t y = 0; y < small.height; ++y)
                for (std::uint32_t x = 0; x < small.width; ++x)
                    for (std::uint32_t c = 0; c < 4; ++c)
                    {
                        // Average the 2x2 block above (clamped at the edge of odd or 1-pixel sizes).
                        int sum = 0;
                        for (std::uint32_t dy = 0; dy < 2; ++dy)
                            for (std::uint32_t dx = 0; dx < 2; ++dx)
                            {
                                const std::uint32_t bx = std::min(x * 2 + dx, big.width - 1);
                                const std::uint32_t by = std::min(y * 2 + dy, big.height - 1);
                                sum += big.rgba[(std::size_t{by} * big.width + bx) * 4 + c];
                            }
                        small.rgba[(std::size_t{y} * small.width + x) * 4 + c] = static_cast<std::uint8_t>((sum + 2) / 4);
                    }
            levels.push_back(std::move(small));
        }
        return levels;
    }
}
