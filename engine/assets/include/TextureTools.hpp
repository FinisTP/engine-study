#pragma once

#include <MeshData.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace umbra::assets
{
    // Image files and tileable-texture helpers (roadmap P8C). Math in TextureTools.md.

    // PNG/JPEG/BMP bytes -> RGBA pixels. False with a reason on failure.
    bool DecodeImage(const std::vector<std::uint8_t> &bytes, core::TextureData &texture, std::string &error);
    bool LoadImageFile(const std::filesystem::path &file, core::TextureData &texture, std::string &error);
    // Width and height without decoding the pixels. False if the file isn't a readable image.
    bool ImageSize(const std::filesystem::path &file, int &width, int &height);
    // RGBA pixels -> PNG bytes.
    bool EncodePng(const core::TextureData &texture, std::vector<std::uint8_t> &png);

    // Bilinear resize (used to make generated images a power-of-two size).
    [[nodiscard]] core::TextureData Resize(const core::TextureData &texture, std::uint32_t width, std::uint32_t height);

    // Makes the left/right and top/bottom edges continue into each other, so the texture repeats
    // without a visible seam. `band` is the width of the blend zone as a fraction of half the image.
    void MakeSeamless(core::TextureData &texture, float band = 0.3F);

    // How much the wrap-around edges differ compared with ordinary neighbouring pixels:
    // about 1 = no visible seam; above visibleSeam the seam is likely to show.
    [[nodiscard]] float SeamScore(const core::TextureData &texture);
    inline constexpr float visibleSeam = 2.0F;

    // Texture rules for materials: power-of-two sides from 64 to 2048.
    [[nodiscard]] bool GoodTextureSize(int width, int height);
}
