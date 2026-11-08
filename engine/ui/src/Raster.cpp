#include <Raster.hpp>

#include <algorithm>
#include <cmath>

namespace umbra::ui
{
    namespace
    {
        glm::vec4 Unpack(std::uint32_t c) // ImGui order: R in the low byte
        {
            return glm::vec4(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF, c >> 24) / 255.0F;
        }

        glm::vec4 Sample(const core::TextureData *texture, glm::vec2 uv) // nearest texel
        {
            if (texture == nullptr || texture->width == 0)
                return glm::vec4{1.0F};
            const auto x = std::clamp(static_cast<int>(uv.x * static_cast<float>(texture->width)), 0, static_cast<int>(texture->width) - 1);
            const auto y = std::clamp(static_cast<int>(uv.y * static_cast<float>(texture->height)), 0, static_cast<int>(texture->height) - 1);
            const std::uint8_t *p = &texture->rgba[(static_cast<std::size_t>(y) * texture->width + static_cast<std::size_t>(x)) * 4];
            return glm::vec4(p[0], p[1], p[2], p[3]) / 255.0F;
        }

        float Edge(glm::vec2 a, glm::vec2 b, glm::vec2 p) { return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x); }
    }

    core::TextureData Rasterize(const core::UiDrawList &list, int width, int height, const TextureLookup &textures, glm::vec4 background)
    {
        // Work in floats, convert to bytes at the end.
        std::vector<glm::vec4> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), background);
        for (const core::UiBatch &batch : list.batches)
        {
            const core::TextureData *texture = textures ? textures(batch.texture) : nullptr;
            // The clip rectangle (x0, y0, x1, y1) limits the pixels this batch may touch.
            const int clipX0 = std::max(0, static_cast<int>(std::floor(batch.clip.x)));
            const int clipY0 = std::max(0, static_cast<int>(std::floor(batch.clip.y)));
            const int clipX1 = std::min(width, static_cast<int>(std::ceil(batch.clip.z)));
            const int clipY1 = std::min(height, static_cast<int>(std::ceil(batch.clip.w)));
            for (std::uint32_t i = 0; i + 2 < batch.indexCount; i += 3)
            {
                // 1. The triangle and its pixel bounds (inside the clip).
                const core::UiVertex *v[3];
                for (int k = 0; k < 3; ++k)
                    v[k] = &list.vertices[batch.firstVertex + list.indices[batch.firstIndex + i + static_cast<std::uint32_t>(k)]];
                const float area = Edge(v[0]->position, v[1]->position, v[2]->position);
                if (std::abs(area) < 1e-6F)
                    continue;
                const int x0 = std::max(clipX0, static_cast<int>(std::floor(std::min({v[0]->position.x, v[1]->position.x, v[2]->position.x}))));
                const int y0 = std::max(clipY0, static_cast<int>(std::floor(std::min({v[0]->position.y, v[1]->position.y, v[2]->position.y}))));
                const int x1 = std::min(clipX1, static_cast<int>(std::ceil(std::max({v[0]->position.x, v[1]->position.x, v[2]->position.x}))));
                const int y1 = std::min(clipY1, static_cast<int>(std::ceil(std::max({v[0]->position.y, v[1]->position.y, v[2]->position.y}))));
                // 2. Each pixel center inside the triangle: weights -> uv and color -> blend.
                for (int y = y0; y < y1; ++y)
                    for (int x = x0; x < x1; ++x)
                    {
                        const glm::vec2 p{static_cast<float>(x) + 0.5F, static_cast<float>(y) + 0.5F};
                        const float w0 = Edge(v[1]->position, v[2]->position, p) / area;
                        const float w1 = Edge(v[2]->position, v[0]->position, p) / area;
                        const float w2 = 1.0F - w0 - w1;
                        if (w0 < 0.0F || w1 < 0.0F || w2 < 0.0F)
                            continue;
                        const glm::vec2 uv = v[0]->uv * w0 + v[1]->uv * w1 + v[2]->uv * w2;
                        const glm::vec4 color = (Unpack(v[0]->color) * w0 + Unpack(v[1]->color) * w1 + Unpack(v[2]->color) * w2) * Sample(texture, uv);
                        glm::vec4 &out = pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
                        out = glm::vec4(glm::vec3(color) * color.a + glm::vec3(out) * (1.0F - color.a), 1.0F);
                    }
            }
        }
        core::TextureData image{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), {}};
        image.rgba.reserve(pixels.size() * 4);
        for (const glm::vec4 &p : pixels)
            for (int k = 0; k < 4; ++k)
                image.rgba.push_back(static_cast<std::uint8_t>(std::clamp(p[k], 0.0F, 1.0F) * 255.0F + 0.5F));
        return image;
    }
}
