#pragma once

#include <string>

#include <DrawList.hpp>
#include <Font.hpp>

#include <glm/vec4.hpp>

#include <string_view>
#include <vector>

namespace umbra::ui
{
    struct Rect final
    {
        float x = 0.0F, y = 0.0F, w = 0.0F, h = 0.0F;
        [[nodiscard]] bool Contains(glm::vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
    };

    // RGBA floats (0..1) -> the packed RGBA8 the renderers read.
    [[nodiscard]] std::uint32_t PackColor(glm::vec4 color);

    // Writes UI triangles into a core::UiDrawList: rectangles, frames, images, 9-slice panels and
    // text, clipped to the current clip rectangle. Consecutive shapes with the same texture and
    // clip share a batch.
    // Where a picture goes in a box and which part of it shows (UI.md "Pictures"). fit: "" stretch,
    // "contain" all of it inside the box, "cover" the box filled and the middle shown, "top" the same from the top.
    struct Fitted final
    {
        Rect rect;
        glm::vec2 uv0{0.0F}, uv1{1.0F};
    };
    Fitted FitPicture(Rect box, glm::vec2 size, const std::string &fit);

    class Painter final
    {
    public:
        Painter(core::UiDrawList &out, const FontAtlas &fonts, core::TextureHandle atlas) : out_(out), fonts_(fonts), atlas_(atlas) {}

        void Fill(Rect r, glm::vec4 color);
        void Frame(Rect r, glm::vec4 color, float width); // an outline inside r
        void Image(Rect r, core::TextureHandle texture, glm::vec4 color, glm::vec2 uv0 = {0, 0}, glm::vec2 uv1 = {1, 1});
        // A panel image whose corners keep their size: `border` = left, top, right, bottom in texture pixels.
        void NineSlice(Rect r, core::TextureHandle texture, glm::vec2 textureSize, glm::vec4 border, glm::vec4 color);
        // Text inside r: wrapped at r.w when wrap is set, each line aligned (0 left, 0.5 center, 1 right).
        void Text(const Face &face, Rect r, std::string_view text, glm::vec4 color, bool wrap, float align);

        void PushClip(Rect r);
        void PopClip();
        // Fonts built `scale` times larger than the layout's units (UI.md "UI scale"): glyphs drawn 1/scale.
        void SetFontScale(float scale) { unit_ = 1.0F / scale; }

    private:
        void Quad(core::TextureHandle texture, glm::vec2 p0, glm::vec2 p1, glm::vec2 uv0, glm::vec2 uv1, std::uint32_t color);

        core::UiDrawList &out_;
        const FontAtlas &fonts_;
        core::TextureHandle atlas_;
        std::vector<glm::vec4> clips_;
        float unit_ = 1.0F; // layout units per font pixel
    };
}
