#include "Post.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <algorithm>
#include <cmath>

namespace umbra::core
{
    int BloomLevels(PostQuality quality) { return quality == PostQuality::Full ? 6 : 4; }
    int BloomStartDivisor(PostQuality quality) { return quality == PostQuality::Full ? 2 : 4; }

    float SrgbToLinear(float c)
    {
        c = std::clamp(c, 0.0F, 1.0F);
        return c <= 0.04045F ? c / 12.92F : std::pow((c + 0.055F) / 1.055F, 2.4F);
    }

    float LinearToSrgb(float c)
    {
        c = std::clamp(c, 0.0F, 1.0F);
        return c <= 0.0031308F ? c * 12.92F : 1.055F * std::pow(c, 1.0F / 2.4F) - 0.055F;
    }

    std::string CheckLutImage(const TextureData &image)
    {
        if (image.width != lutSize * lutSize || image.height != lutSize)
            return "a LUT image must be 256x16 (16 slices of 16x16)";
        return {};
    }

    TextureData BakeGradeLut(const PostSettings &settings, const TextureData *lutImage)
    {
        TextureData lut{lutSize * lutSize, lutSize, std::vector<std::uint8_t>(static_cast<std::size_t>(lutSize * lutSize * lutSize) * 4)};
        const bool useImage = lutImage != nullptr && CheckLutImage(*lutImage).empty();
        // Warm = more red, less blue; cold the other way (a simple white-balance shift).
        const float t = std::clamp(settings.temperature, -1.0F, 1.0F);
        const glm::vec3 balance{1.0F + 0.12F * t, 1.0F + 0.02F * t, 1.0F - 0.15F * t};
        for (int b = 0; b < lutSize; ++b)
            for (int g = 0; g < lutSize; ++g)
                for (int r = 0; r < lutSize; ++r)
                {
                    const std::size_t at = (static_cast<std::size_t>(g) * lutSize * lutSize + static_cast<std::size_t>(b) * lutSize + r) * 4;
                    // 1. The cell's color (sRGB), through the artist's LUT image if there is one.
                    glm::vec3 s = glm::vec3(r, g, b) / float(lutSize - 1);
                    if (useImage)
                        s = glm::vec3(lutImage->rgba[at], lutImage->rgba[at + 1], lutImage->rgba[at + 2]) / 255.0F;
                    // 2. White balance and saturation in linear light.
                    glm::vec3 c{SrgbToLinear(s.r), SrgbToLinear(s.g), SrgbToLinear(s.b)};
                    c *= balance;
                    const float luma = glm::dot(c, glm::vec3{0.2126F, 0.7152F, 0.0722F});
                    c = glm::mix(glm::vec3(luma), c, settings.saturation);
                    // 3. Contrast around middle gray, in sRGB (where it looks even).
                    for (int k = 0; k < 3; ++k)
                    {
                        const float v = (LinearToSrgb(c[k]) - 0.5F) * settings.contrast + 0.5F;
                        lut.rgba[at + k] = static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0F, 1.0F) * 255.0F));
                    }
                    lut.rgba[at + 3] = 255;
                }
        return lut;
    }

    PostBlock PackPost(const PostSettings &settings, std::uint32_t width, std::uint32_t height)
    {
        PostBlock p;
        p.exposure = {std::exp2(settings.exposure), std::max(settings.bloom, 0.0F), std::max(settings.bloomThreshold, 0.0F),
                      std::clamp(settings.vignette, 0.0F, 1.0F)};
        p.sizes = {1.0F / static_cast<float>(std::max(width, 1u)), 1.0F / static_cast<float>(std::max(height, 1u)), float(lutSize), 0.0F};
        return p;
    }

    bool AnalogOn(const PostSettings &settings) { return settings.crt > 0.0F || settings.vhs > 0.0F; }

    PostBlock PackAnalog(const PostSettings &settings, std::uint32_t width, std::uint32_t height, float seconds)
    {
        const float w = static_cast<float>(std::max(width, 1u)), h = static_cast<float>(std::max(height, 1u));
        PostBlock p;
        p.exposure = {std::clamp(settings.crt, 0.0F, 1.0F), std::clamp(settings.vhs, 0.0F, 1.0F), std::fmod(std::max(seconds, 0.0F), 1000.0F), 0.0F};
        p.sizes = {1.0F / w, 1.0F / h, w, h};
        return p;
    }

    glm::vec2 CrtWarp(glm::vec2 uv, float crt)
    {
        // Centre at 0, push each point out by how far it is from the centre (squared).
        const glm::vec2 c = uv * 2.0F - 1.0F;
        return c * (1.0F + crtBulge * std::clamp(crt, 0.0F, 1.0F) * glm::dot(c, c)) * 0.5F + 0.5F;
    }

    glm::vec3 Tonemap(glm::vec3 hdr)
    {
        // ACES filmic curve (Narkowicz fit), with 0.8 so middle gray stays about where it was.
        const glm::vec3 x = glm::max(hdr, glm::vec3(0.0F)) * 0.8F;
        return glm::clamp(x * (2.51F * x + 0.03F) / (x * (2.43F * x + 0.59F) + 0.14F), 0.0F, 1.0F);
    }

    glm::vec3 ApplyGrade(const TextureData &lut, glm::vec3 linear)
    {
        // Look up the table at the color's sRGB value, blending the 8 nearest cells (like the GPU).
        const glm::vec3 p = glm::vec3(LinearToSrgb(linear.r), LinearToSrgb(linear.g), LinearToSrgb(linear.b)) * float(lutSize - 1);
        const glm::ivec3 i0 = glm::clamp(glm::ivec3(glm::floor(p)), 0, lutSize - 1), i1 = glm::min(i0 + 1, lutSize - 1);
        const glm::vec3 f = p - glm::vec3(i0);
        const auto cell = [&](int r, int g, int b) {
            const std::size_t at = (static_cast<std::size_t>(g) * lutSize * lutSize + static_cast<std::size_t>(b) * lutSize + r) * 4;
            return glm::vec3(lut.rgba[at], lut.rgba[at + 1], lut.rgba[at + 2]) / 255.0F;
        };
        glm::vec3 s{0.0F};
        for (int k = 0; k < 8; ++k)
        {
            const glm::ivec3 c{k & 1 ? i1.x : i0.x, k & 2 ? i1.y : i0.y, k & 4 ? i1.z : i0.z};
            const float w = (k & 1 ? f.x : 1 - f.x) * (k & 2 ? f.y : 1 - f.y) * (k & 4 ? f.z : 1 - f.z);
            s += w * cell(c.x, c.y, c.z);
        }
        return {SrgbToLinear(s.r), SrgbToLinear(s.g), SrgbToLinear(s.b)};
    }

    glm::vec3 PostPixel(const PostSettings &settings, const TextureData &lut, glm::vec3 hdr, glm::vec3 bloom, float u, float v)
    {
        // 1. Exposure and bloom in HDR, 2. tonemap to 0..1, 3. grade, 4. vignette.
        const glm::vec3 lit = hdr * std::exp2(settings.exposure) + bloom * std::max(settings.bloom, 0.0F);
        glm::vec3 c = ApplyGrade(lut, Tonemap(lit));
        const float d = glm::length(glm::vec2(u - 0.5F, v - 0.5F)) * 1.41421356F; // 0 middle, 1 corner
        return c * (1.0F - std::clamp(settings.vignette, 0.0F, 1.0F) * d * d);
    }
}
