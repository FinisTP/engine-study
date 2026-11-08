#include <Painter.hpp>

#include <algorithm>
#include <cmath>

namespace umbra::ui
{
    std::uint32_t PackColor(glm::vec4 color)
    {
        const auto channel = [](float v) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.0F, 1.0F) * 255.0F)); };
        return channel(color.r) | channel(color.g) << 8 | channel(color.b) << 16 | channel(color.a) << 24;
    }

    void Painter::Quad(core::TextureHandle texture, glm::vec2 p0, glm::vec2 p1, glm::vec2 uv0, glm::vec2 uv1, std::uint32_t color)
    {
        // 1. Same texture and clip as the last batch (and room for 4 more vertices): add to it.
        const glm::vec4 clip = clips_.empty() ? glm::vec4{0.0F, 0.0F, 1e9F, 1e9F} : clips_.back();
        const auto vertexCount = static_cast<std::uint32_t>(out_.vertices.size());
        if (out_.batches.empty() || out_.batches.back().texture != texture || out_.batches.back().clip != clip ||
            vertexCount - out_.batches.back().firstVertex > 65532)
            out_.batches.push_back({texture, clip, vertexCount, static_cast<std::uint32_t>(out_.indices.size()), 0});
        core::UiBatch &batch = out_.batches.back();
        // 2. Two triangles: 0-1-2, 0-2-3 (top left, top right, bottom right, bottom left).
        const auto base = static_cast<std::uint16_t>(vertexCount - batch.firstVertex);
        out_.vertices.push_back({p0, uv0, color});
        out_.vertices.push_back({{p1.x, p0.y}, {uv1.x, uv0.y}, color});
        out_.vertices.push_back({p1, uv1, color});
        out_.vertices.push_back({{p0.x, p1.y}, {uv0.x, uv1.y}, color});
        for (const int i : {0, 1, 2, 0, 2, 3})
            out_.indices.push_back(static_cast<std::uint16_t>(base + i));
        batch.indexCount += 6;
    }

    void Painter::Fill(Rect r, glm::vec4 color)
    {
        if (color.a <= 0.0F || r.w <= 0.0F || r.h <= 0.0F)
            return;
        const glm::vec2 white = fonts_.WhiteUv();
        Quad(atlas_, {r.x, r.y}, {r.x + r.w, r.y + r.h}, white, white, PackColor(color));
    }

    void Painter::Frame(Rect r, glm::vec4 color, float width)
    {
        if (width <= 0.0F)
            return;
        Fill({r.x, r.y, r.w, width}, color);
        Fill({r.x, r.y + r.h - width, r.w, width}, color);
        Fill({r.x, r.y + width, width, r.h - 2 * width}, color);
        Fill({r.x + r.w - width, r.y + width, width, r.h - 2 * width}, color);
    }

    void Painter::Image(Rect r, core::TextureHandle texture, glm::vec4 color, glm::vec2 uv0, glm::vec2 uv1)
    {
        Quad(texture, {r.x, r.y}, {r.x + r.w, r.y + r.h}, uv0, uv1, PackColor(color));
    }

    void Painter::NineSlice(Rect r, core::TextureHandle texture, glm::vec2 size, glm::vec4 border, glm::vec4 color)
    {
        // Three columns and three rows: fixed-size corners, stretched edges and middle.
        const float xs[] = {r.x, r.x + border.x, r.x + r.w - border.z, r.x + r.w};
        const float ys[] = {r.y, r.y + border.y, r.y + r.h - border.w, r.y + r.h};
        const float us[] = {0.0F, border.x / size.x, 1.0F - border.z / size.x, 1.0F};
        const float vs[] = {0.0F, border.y / size.y, 1.0F - border.w / size.y, 1.0F};
        const std::uint32_t packed = PackColor(color);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                if (xs[col + 1] > xs[col] && ys[row + 1] > ys[row])
                    Quad(texture, {xs[col], ys[row]}, {xs[col + 1], ys[row + 1]}, {us[col], vs[row]}, {us[col + 1], vs[row + 1]}, packed);
    }

    void Painter::Text(const Face &face, Rect r, std::string_view text, glm::vec4 color, bool wrap, float align)
    {
        const std::uint32_t packed = PackColor(color);
        float y = r.y + face.ascent;
        for (const std::string &line : FontAtlas::Lines(face, text, wrap ? r.w : 0.0F))
        {
            // Each line starts where `align` puts it; pen positions are rounded to whole pixels (crisp).
            const float width = FontAtlas::Measure(face, line).x;
            float x = std::round(r.x + (r.w - width) * align);
            for (std::size_t i = 0; i < line.size();)
            {
                const Glyph *g = face.Find(NextCodePoint(line, i));
                if (g == nullptr)
                    continue;
                if (g->max.x > g->min.x)
                    Quad(atlas_, {x + g->min.x, y + g->min.y}, {x + g->max.x, y + g->max.y}, g->uvMin, g->uvMax, packed);
                x += g->advance;
            }
            y += face.lineHeight;
        }
    }

    void Painter::PushClip(Rect r)
    {
        glm::vec4 clip{r.x, r.y, r.x + r.w, r.y + r.h};
        if (!clips_.empty()) // inside the parent's clip
            clip = {std::max(clip.x, clips_.back().x), std::max(clip.y, clips_.back().y), std::min(clip.z, clips_.back().z), std::min(clip.w, clips_.back().w)};
        clips_.push_back(clip);
    }

    void Painter::PopClip()
    {
        if (!clips_.empty())
            clips_.pop_back();
    }
}
