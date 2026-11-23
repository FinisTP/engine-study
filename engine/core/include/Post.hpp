#pragma once

#include <MeshData.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <string>
#include <vector>

// Post-processing (roadmap S7): what happens to the lit HDR picture before it reaches the screen.
// The scene's "post" component sets it; both renderers use these numbers. The math: Post.md.
namespace umbra::core
{
    // The scene's "post" component.
    struct PostSettings final
    {
        float exposure = 0.0F;       // stops: +1 = twice as bright
        float bloom = 0.5F;          // how much bright light bleeds around (0 = none)
        float bloomThreshold = 1.0F; // light brighter than this blooms
        float contrast = 1.0F;
        float saturation = 1.0F;     // 0 = gray, 1 = as is
        float temperature = 0.0F;    // -1 cold (blue) .. +1 warm (orange)
        float vignette = 0.25F;      // darker corners, 0 = none
        std::string lut;             // optional color table image (256x16), asset path
        float crt = 0.0F;            // the analog screen (Post.md "6"): an old CRT, 0 = off .. 1 = strong
        float vhs = 0.0F;            // a worn VHS tape, 0 = off .. 1 = strong
    };

    // How much post work the device does (user decision 2026-11-09: phones get Reduced).
    enum class PostQuality
    {
        Full,   // bloom from half resolution, 6 levels
        Reduced // bloom from quarter resolution, 4 levels
    };
    [[nodiscard]] int BloomLevels(PostQuality quality);
    [[nodiscard]] int BloomStartDivisor(PostQuality quality); // 2 = half the screen size

    // The color table: lutSize^3 cells, RGBA8, sRGB in -> sRGB out. Cell (r, g, b) is at
    // x = b * lutSize + r, y = g of a 256x16 strip, the usual layout for LUT images.
    inline constexpr int lutSize = 16;
    // Bakes the grade (an optional LUT image first, then temperature, saturation, contrast) into one table.
    [[nodiscard]] TextureData BakeGradeLut(const PostSettings &settings, const TextureData *lutImage = nullptr);
    // "" if `image` can be used as a LUT, otherwise why not.
    [[nodiscard]] std::string CheckLutImage(const TextureData &image);

    // What the composite pass uploads.
    struct PostBlock final
    {
        glm::vec4 exposure{1.0F, 0.5F, 1.0F, 0.25F}; // x = 2^exposure, y = bloom, z = threshold, w = vignette
        glm::vec4 sizes{0.0F};                        // xy = 1 / screen size, z = lutSize
    };
    [[nodiscard]] PostBlock PackPost(const PostSettings &settings, std::uint32_t width, std::uint32_t height);

    // The analog screen (Post.md "6"): a last pass over the finished frame, menus and text included.
    inline constexpr float crtBulge = 0.08F; // the CRT's curve at full strength (Post.hlsl uses the same)
    [[nodiscard]] bool AnalogOn(const PostSettings &settings);
    // What it uploads: exposure = (crt, vhs, seconds, 0), sizes = (1/width, 1/height, width, height).
    [[nodiscard]] PostBlock PackAnalog(const PostSettings &settings, std::uint32_t width, std::uint32_t height, float seconds);
    // The CRT's bulge, as the shader does it: where on the picture the screen point `uv` (0..1) looks.
    [[nodiscard]] glm::vec2 CrtWarp(glm::vec2 uv, float crt);

    // The same steps the GPU does, on one pixel (for tests and pictures): tonemap, grade, vignette.
    // `uv` 0..1 across the screen; returns linear color 0..1.
    [[nodiscard]] glm::vec3 Tonemap(glm::vec3 hdr);
    [[nodiscard]] glm::vec3 ApplyGrade(const TextureData &lut, glm::vec3 linear);
    [[nodiscard]] glm::vec3 PostPixel(const PostSettings &settings, const TextureData &lut, glm::vec3 hdr, glm::vec3 bloom, float u, float v);

    [[nodiscard]] float SrgbToLinear(float c);
    [[nodiscard]] float LinearToSrgb(float c);
}
