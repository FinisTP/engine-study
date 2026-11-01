#include <MeshData.hpp>

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
}
