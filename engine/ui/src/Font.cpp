#include <Font.hpp>

// stb_truetype (public domain, third_party/): compiled here, its own warnings silenced.
#if defined(_MSC_VER)
#pragma warning(push, 0)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "../third_party/stb_truetype.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cmath>

namespace umbra::ui
{
    namespace
    {
        // Characters baked for every face: ASCII, Latin-1, Latin Extended-A/B, Vietnamese, punctuation.
        const std::pair<int, int> ranges[] = {{0x20, 0x7E}, {0xA0, 0x24F}, {0x1EA0, 0x1EF9}, {0x2010, 0x2026}};
    }

    const Glyph *Face::Find(char32_t c) const
    {
        const auto it = glyphs.find(c);
        if (it != glyphs.end())
            return &it->second;
        const auto question = glyphs.find(U'?');
        return question == glyphs.end() ? nullptr : &question->second;
    }

    char32_t NextCodePoint(std::string_view text, std::size_t &at)
    {
        const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
        const unsigned char first = byte(at);
        const int length = first < 0x80 ? 1 : (first >> 5) == 6 ? 2 : (first >> 4) == 14 ? 3 : (first >> 3) == 30 ? 4 : 0;
        if (length == 0 || at + static_cast<std::size_t>(length) > text.size())
        {
            ++at;
            return 0xFFFD;
        }
        char32_t c = length == 1 ? first : first & (0x7F >> length);
        for (int i = 1; i < length; ++i)
            c = (c << 6) | (byte(at + static_cast<std::size_t>(i)) & 0x3F);
        at += static_cast<std::size_t>(length);
        return c;
    }

    bool FontAtlas::Build(const std::vector<FontSource> &sources, std::vector<core::Diagnostic> &diagnostics)
    {
        // 1. Try a 1024 atlas, then bigger ones, until every face fits.
        for (int side = 1024; side <= 4096; side *= 2)
        {
            std::vector<unsigned char> alpha(static_cast<std::size_t>(side * side), 0);
            stbtt_pack_context pack;
            if (!stbtt_PackBegin(&pack, alpha.data(), side, side, 0, 1, nullptr))
                return false;
            stbtt_PackSetOversampling(&pack, 1, 1);
            bool fits = true;
            faces_.clear();
            for (const FontSource &source : sources)
            {
                stbtt_fontinfo info;
                // A font file starts with a known 4-byte tag; stb_truetype trusts its input, so check first.
                const auto tag = [&](const char *t) { return source.ttf.size() >= 64 && std::equal(t, t + 4, source.ttf.begin()); };
                const bool known = tag("\x00\x01\x00\x00") || tag("true") || tag("OTTO") || tag("ttcf");
                if (!known || !stbtt_InitFont(&info, source.ttf.data(), stbtt_GetFontOffsetForIndex(source.ttf.data(), 0)))
                {
                    diagnostics.push_back({core::Severity::Error, "ui.font", source.name, "", "not a TrueType font"});
                    stbtt_PackEnd(&pack);
                    return false;
                }
                // 2. Pack each range.
                Face face;
                face.size = source.size;
                int ascent = 0, descent = 0, gap = 0;
                stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
                const float scale = stbtt_ScaleForPixelHeight(&info, source.size);
                face.ascent = std::round(static_cast<float>(ascent) * scale);
                face.lineHeight = std::round(static_cast<float>(ascent - descent + gap) * scale);
                for (const auto &[first, last] : ranges)
                {
                    std::vector<stbtt_packedchar> chars(static_cast<std::size_t>(last - first + 1));
                    stbtt_pack_range range{source.size, first, nullptr, last - first + 1, chars.data(), 0, 0};
                    if (!stbtt_PackFontRanges(&pack, source.ttf.data(), 0, &range, 1))
                        fits = fits && first != 0x20 && side == 4096; // a full atlas grows; only the largest may drop optional letters
                    for (int c = first; c <= last; ++c)
                    {
                        const stbtt_packedchar &p = chars[static_cast<std::size_t>(c - first)];
                        if (p.x1 == p.x0 && c != 0x20)
                            continue; // not in this font
                        const float inv = 1.0F / static_cast<float>(side);
                        face.glyphs[static_cast<char32_t>(c)] = {{p.xoff, p.yoff}, {p.xoff2, p.yoff2},
                                                                 {static_cast<float>(p.x0) * inv, static_cast<float>(p.y0) * inv},
                                                                 {static_cast<float>(p.x1) * inv, static_cast<float>(p.y1) * inv}, p.xadvance};
                    }
                }
                faces_[source.name] = std::move(face);
            }
            stbtt_PackEnd(&pack);
            // The white block for plain rectangles: the bottom-right 2x2, which must still be free.
            const std::size_t last = static_cast<std::size_t>(side) * static_cast<std::size_t>(side) - 1, row = static_cast<std::size_t>(side);
            for (const std::size_t i : {last, last - 1, last - row, last - row - 1})
                fits = fits && alpha[i] == 0;
            if (!fits)
                continue;
            for (const std::size_t i : {last, last - 1, last - row, last - row - 1})
                alpha[i] = 255;
            // 3. White RGB, coverage in alpha.
            image_.width = image_.height = static_cast<std::uint32_t>(side);
            image_.rgba.assign(alpha.size() * 4, 255);
            for (std::size_t i = 0; i < alpha.size(); ++i)
                image_.rgba[i * 4 + 3] = alpha[i];
            return true;
        }
        diagnostics.push_back({core::Severity::Error, "ui.font", "", "", "the fonts do not fit in a 4096 x 4096 atlas"});
        return false;
    }

    const Face *FontAtlas::Find(const std::string &name) const
    {
        const auto it = faces_.find(name);
        return it == faces_.end() ? nullptr : &it->second;
    }

    std::vector<std::string> FontAtlas::Lines(const Face &face, std::string_view text, float wrap)
    {
        // Words go on the current line until the next one would pass `wrap`; '\n' always breaks.
        std::vector<std::string> lines(1);
        float width = 0.0F;
        std::size_t at = 0;
        while (at < text.size())
        {
            if (text[at] == '\n')
            {
                lines.emplace_back();
                width = 0.0F;
                ++at;
                continue;
            }
            const std::size_t end = std::min(text.find_first_of(" \n", at), text.size());
            const std::string_view word = text.substr(at, end - at);
            float wordWidth = 0.0F;
            for (std::size_t i = 0; i < word.size();)
                if (const Glyph *g = face.Find(NextCodePoint(word, i)))
                    wordWidth += g->advance;
            const Glyph *space = face.Find(U' ');
            const float spaceWidth = space ? space->advance : 0.0F;
            if (wrap > 0.0F && width > 0.0F && width + spaceWidth + wordWidth > wrap)
            {
                lines.emplace_back();
                width = 0.0F;
            }
            if (width > 0.0F)
            {
                lines.back() += ' ';
                width += spaceWidth;
            }
            lines.back() += word;
            width += wordWidth;
            at = end < text.size() && text[end] == ' ' ? end + 1 : end;
        }
        return lines;
    }

    glm::vec2 FontAtlas::Measure(const Face &face, std::string_view text, float wrap)
    {
        glm::vec2 size{0.0F, 0.0F};
        for (const std::string &line : Lines(face, text, wrap))
        {
            float width = 0.0F;
            for (std::size_t i = 0; i < line.size();)
                if (const Glyph *g = face.Find(NextCodePoint(line, i)))
                    width += g->advance;
            size.x = std::max(size.x, width);
            size.y += face.lineHeight;
        }
        return size;
    }

    std::u32string MissingGlyphs(const std::vector<unsigned char> &ttf, const std::u32string &wanted)
    {
        // 1. Same check as Build: stb_truetype trusts its input.
        const auto tag = [&](const char *t) { return ttf.size() >= 64 && std::equal(t, t + 4, ttf.begin()); };
        stbtt_fontinfo info;
        const bool known = tag("\x00\x01\x00\x00") || tag("true") || tag("OTTO") || tag("ttcf");
        if (!known || !stbtt_InitFont(&info, ttf.data(), stbtt_GetFontOffsetForIndex(ttf.data(), 0)))
            return wanted;
        // 2. Glyph index 0 is the font's "missing" box.
        std::u32string missing;
        for (const char32_t c : wanted)
            if (stbtt_FindGlyphIndex(&info, static_cast<int>(c)) == 0)
                missing += c;
        return missing;
    }
}
