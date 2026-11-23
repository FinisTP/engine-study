#pragma once

#include <Diagnostic.hpp>
#include <MeshData.hpp>

#include <glm/vec2.hpp>

#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace umbra::ui
{
    // One drawn character: where its quad goes relative to the pen (pixels) and where it is in the atlas.
    struct Glyph final
    {
        glm::vec2 min{0.0F}, max{0.0F};     // quad, relative to the pen on the baseline
        glm::vec2 uvMin{0.0F}, uvMax{0.0F}; // atlas coordinates
        float advance = 0.0F;
    };

    // A TTF at one pixel size.
    struct Face final
    {
        float size = 0.0F;
        float ascent = 0.0F, lineHeight = 0.0F;
        std::unordered_map<char32_t, Glyph> glyphs;

        [[nodiscard]] const Glyph *Find(char32_t c) const;
    };

    struct FontSource final
    {
        std::string name;       // "body", "title" (used by the skin)
        std::vector<unsigned char> ttf;
        float size = 18.0F;     // pixels
    };

    // All faces packed into one white-on-transparent texture (Font.md). The bottom-right 2x2
    // pixels are solid white, so plain rectangles use the same texture as text.
    class FontAtlas final
    {
    public:
        bool Build(const std::vector<FontSource> &sources, std::vector<core::Diagnostic> &diagnostics);

        [[nodiscard]] const Face *Find(const std::string &name) const;
        [[nodiscard]] const core::TextureData &Image() const { return image_; }
        [[nodiscard]] glm::vec2 WhiteUv() const { return {1.0F - 1.0F / static_cast<float>(image_.width), 1.0F - 1.0F / static_cast<float>(image_.height)}; }

        // Text size in pixels; `wrap` > 0 breaks lines at spaces to stay within that width.
        [[nodiscard]] static glm::vec2 Measure(const Face &face, std::string_view text, float wrap = 0.0F);
        // The text split into lines as Measure/drawing does it.
        [[nodiscard]] static std::vector<std::string> Lines(const Face &face, std::string_view text, float wrap);

    private:
        std::map<std::string, Face> faces_;
        core::TextureData image_;
    };

    // Next code point of UTF-8 text (bad bytes become U+FFFD); advances `at`.
    char32_t NextCodePoint(std::string_view text, std::size_t &at);

    // The code points of `wanted` the font has no glyph for (all of them if `ttf` is not a font).
    [[nodiscard]] std::u32string MissingGlyphs(const std::vector<unsigned char> &ttf, const std::u32string &wanted);
}
