#include <TextureTools.hpp>

// The stb implementations live in GltfImporter.cpp (tinygltf builds them); only declarations here.
#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace umbra::assets
{
    namespace
    {
        std::uint8_t Pixel(const core::TextureData &t, std::uint32_t x, std::uint32_t y, std::uint32_t c)
        {
            return t.rgba[(std::size_t{y} * t.width + x) * 4 + c];
        }

        // Average absolute difference over RGB.
        float Difference(const core::TextureData &t, std::uint32_t x0, std::uint32_t y0, std::uint32_t x1, std::uint32_t y1)
        {
            float sum = 0.0F;
            for (std::uint32_t c = 0; c < 3; ++c)
                sum += std::abs(static_cast<float>(Pixel(t, x0, y0, c)) - static_cast<float>(Pixel(t, x1, y1, c)));
            return sum / 3.0F;
        }
    }

    bool DecodeImage(const std::vector<std::uint8_t> &bytes, core::TextureData &texture, std::string &error)
    {
        int width = 0, height = 0, channels = 0;
        stbi_uc *pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 4);
        if (pixels == nullptr)
        {
            error = std::string("not a readable image: ") + stbi_failure_reason();
            return false;
        }
        texture.width = static_cast<std::uint32_t>(width);
        texture.height = static_cast<std::uint32_t>(height);
        texture.rgba.assign(pixels, pixels + static_cast<std::size_t>(width) * height * 4);
        stbi_image_free(pixels);
        error = core::ValidateTexture(texture);
        return error.empty();
    }

    bool LoadImageFile(const std::filesystem::path &file, core::TextureData &texture, std::string &error)
    {
        std::error_code ec;
        std::ifstream in(file, std::ios::binary);
        if (!in || !std::filesystem::is_regular_file(file, ec)) // a folder opens on Linux, then throws on read
        {
            error = "cannot open " + file.string();
            return false;
        }
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
        return DecodeImage(bytes, texture, error);
    }

    bool ImageSize(const std::filesystem::path &file, int &width, int &height)
    {
        std::ifstream in(file, std::ios::binary);
        std::vector<std::uint8_t> head(64 * 1024);
        in.read(reinterpret_cast<char *>(head.data()), static_cast<std::streamsize>(head.size()));
        int channels = 0;
        return in.gcount() > 0 && stbi_info_from_memory(head.data(), static_cast<int>(in.gcount()), &width, &height, &channels) != 0;
    }

    bool EncodePng(const core::TextureData &texture, std::vector<std::uint8_t> &png)
    {
        png.clear();
        auto append = [](void *context, void *data, int size) {
            auto *out = static_cast<std::vector<std::uint8_t> *>(context);
            out->insert(out->end(), static_cast<std::uint8_t *>(data), static_cast<std::uint8_t *>(data) + size);
        };
        return stbi_write_png_to_func(append, &png, static_cast<int>(texture.width), static_cast<int>(texture.height), 4,
                                      texture.rgba.data(), static_cast<int>(texture.width * 4)) != 0;
    }

    core::TextureData Resize(const core::TextureData &texture, std::uint32_t width, std::uint32_t height)
    {
        core::TextureData out{width, height, std::vector<std::uint8_t>(std::size_t{width} * height * 4)};
        for (std::uint32_t y = 0; y < height; ++y)
            for (std::uint32_t x = 0; x < width; ++x)
            {
                // 1. Where this pixel's centre falls in the source image.
                const float sx = std::clamp((static_cast<float>(x) + 0.5F) * static_cast<float>(texture.width) / static_cast<float>(width) - 0.5F,
                                            0.0F, static_cast<float>(texture.width - 1));
                const float sy = std::clamp((static_cast<float>(y) + 0.5F) * static_cast<float>(texture.height) / static_cast<float>(height) - 0.5F,
                                            0.0F, static_cast<float>(texture.height - 1));
                const auto x0 = static_cast<std::uint32_t>(sx), y0 = static_cast<std::uint32_t>(sy);
                const std::uint32_t x1 = std::min(x0 + 1, texture.width - 1), y1 = std::min(y0 + 1, texture.height - 1);
                const float fx = sx - static_cast<float>(x0), fy = sy - static_cast<float>(y0);
                // 2. Mix the four surrounding source pixels.
                for (std::uint32_t c = 0; c < 4; ++c)
                {
                    const float top = Pixel(texture, x0, y0, c) * (1 - fx) + Pixel(texture, x1, y0, c) * fx;
                    const float bottom = Pixel(texture, x0, y1, c) * (1 - fx) + Pixel(texture, x1, y1, c) * fx;
                    out.rgba[(std::size_t{y} * width + x) * 4 + c] = static_cast<std::uint8_t>(std::lround(top * (1 - fy) + bottom * fy));
                }
            }
        return out;
    }

    void MakeSeamless(core::TextureData &texture, float band)
    {
        // One pass per direction: first left/right, then top/bottom (TextureTools.md).
        for (int axis = 0; axis < 2; ++axis)
        {
            const std::uint32_t w = texture.width, h = texture.height;
            const core::TextureData original = texture;
            const float size = static_cast<float>(axis == 0 ? w : h);
            const float bandWidth = std::max(1.0F, band * size / 2.0F);
            for (std::uint32_t y = 0; y < h; ++y)
                for (std::uint32_t x = 0; x < w; ++x)
                {
                    // 1. Weight of the original: 1 on the centre line, fading to 0 over the band.
                    const float position = static_cast<float>(axis == 0 ? x : y);
                    const float keep = std::max(0.0F, 1.0F - std::abs(position - size / 2.0F) / bandWidth);
                    // 2. Elsewhere the image shifted by half, whose edges in this direction wrap without a jump.
                    const std::uint32_t sx = axis == 0 ? (x + w / 2) % w : x;
                    const std::uint32_t sy = axis == 1 ? (y + h / 2) % h : y;
                    for (std::uint32_t c = 0; c < 4; ++c)
                    {
                        const float value = Pixel(original, sx, sy, c) * (1.0F - keep) + Pixel(original, x, y, c) * keep;
                        texture.rgba[(std::size_t{y} * w + x) * 4 + c] = static_cast<std::uint8_t>(std::lround(value));
                    }
                }
        }
    }

    float SeamScore(const core::TextureData &texture)
    {
        const std::uint32_t w = texture.width, h = texture.height;
        if (w < 2 || h < 2)
            return 1.0F;
        // 1. Differences across the wrap-around edges.
        float seam = 0.0F;
        for (std::uint32_t y = 0; y < h; ++y)
            seam += Difference(texture, w - 1, y, 0, y);
        for (std::uint32_t x = 0; x < w; ++x)
            seam += Difference(texture, x, h - 1, x, 0);
        seam /= static_cast<float>(w + h);
        // 2. Typical differences between neighbours inside the image.
        float inner = 0.0F;
        for (std::uint32_t y = 0; y < h; ++y)
            for (std::uint32_t x = 0; x + 1 < w; ++x)
                inner += Difference(texture, x, y, x + 1, y);
        for (std::uint32_t y = 0; y + 1 < h; ++y)
            for (std::uint32_t x = 0; x < w; ++x)
                inner += Difference(texture, x, y, x, y + 1);
        inner /= static_cast<float>((w - 1) * h + w * (h - 1));
        return seam / std::max(inner, 1.0F);
    }

    bool GoodTextureSize(int width, int height)
    {
        auto good = [](int n) { return n >= 64 && n <= 2048 && (n & (n - 1)) == 0; };
        return good(width) && good(height);
    }
}
