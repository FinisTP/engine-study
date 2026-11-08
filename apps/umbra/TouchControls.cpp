#include "TouchControls.hpp"

#include <glm/geometric.hpp>

#include <algorithm>

using umbra::platform::InputKey;

std::vector<TouchControls::Button> TouchControls::Layout(glm::vec2 p, const State &state)
{
    // Sizes follow the screen height (720 px = 1), so a thumb covers the same share on any phone.
    const float s = p.y / 720.0F;
    std::vector<Button> buttons{{"Menu", InputKey::Escape, {p.x - 60 * s, 60 * s}, 38 * s}};
    if (state.exploring)
    {
        buttons.push_back({"Jump", InputKey::Space, {p.x - 110 * s, p.y - 110 * s}, 62 * s});
        buttons.push_back({"Use", InputKey::E, {p.x - 250 * s, p.y - 80 * s}, 50 * s});
        buttons.push_back({"Run", InputKey::LeftShift, {p.x - 100 * s, p.y - 260 * s}, 44 * s});
        buttons.push_back({"Bag", InputKey::I, {p.x - 145 * s, 60 * s}, 34 * s});
        buttons.push_back({"Keys", InputKey::H, {p.x - 225 * s, 60 * s}, 34 * s});
    }
    if (state.realtimeFight)
    {
        // The hotkeys in an arc around the lower right corner, flee above them.
        const glm::vec2 corner{p.x - 70 * s, p.y - 70 * s};
        const InputKey keys[] = {InputKey::Num1, InputKey::Num2, InputKey::Num3, InputKey::Num4};
        const char *labels[] = {"1", "2", "3", "4"};
        for (int i = 0; i < 4; ++i)
        {
            const float angle = 3.14159265F * (1.0F + 0.5F * static_cast<float>(i) / 3.0F); // left -> up
            buttons.push_back({labels[i], keys[i], corner + glm::vec2{std::cos(angle), std::sin(angle)} * (200 * s), 50 * s});
        }
        buttons.push_back({"Flee", InputKey::F, {p.x - 70 * s, p.y - 70 * s}, 48 * s});
    }
    return buttons;
}

void TouchControls::ReleaseAll(umbra::platform::Input &input)
{
    for (const auto &[finger, key] : held_)
        input.HandleKeyUp(key);
    held_.clear();
    stickFinger_.reset();
    lookFinger_.reset();
}

TouchControls::Output TouchControls::Update(umbra::platform::Input &input, glm::vec2 pixels, const State &state, const umbra::ui::FontAtlas &fonts,
                                            umbra::core::TextureHandle atlas, const ImageSource &images, umbra::core::UiDrawList &out)
{
    Output result;
    // 1. Shown once a finger touched the screen, and only while the game itself has the screen.
    const bool show = input.TouchSeen() && !state.menuOpen && (state.exploring || state.realtimeFight);
    if (!show)
    {
        ReleaseAll(input);
        visible_ = false;
        return result;
    }
    visible_ = true;
    const std::vector<Button> buttons = Layout(pixels, state);
    const float s = pixels.y / 720.0F, stickRadius = 90.0F * s;

    // 2. Each finger: a new one on a button holds its key; on the left it becomes the stick,
    //    on the right it drags the camera. A lifted finger lets go of what it had.
    for (const umbra::platform::Finger &f : input.Fingers())
    {
        const glm::vec2 at{f.x * pixels.x, f.y * pixels.y};
        if (f.pressed)
        {
            const auto hit = std::find_if(buttons.begin(), buttons.end(), [&](const Button &b) { return glm::distance(at, b.center) <= b.radius * 1.15F; });
            if (hit != buttons.end())
            {
                held_[f.id] = hit->key;
                input.HandleKeyDown(hit->key, false);
            }
            else if (at.x < pixels.x * 0.45F && !stickFinger_)
                stickFinger_ = f.id, stickBase_ = at;
            else if (!lookFinger_)
                lookFinger_ = f.id;
        }
        if (stickFinger_ == f.id)
        {
            const glm::vec2 offset = at - stickBase_;
            const float length = glm::length(offset);
            const glm::vec2 clamped = length > stickRadius ? offset * (stickRadius / length) : offset;
            result.move = {clamped.x / stickRadius, -clamped.y / stickRadius}; // screen y grows down
        }
        if (lookFinger_ == f.id)
            result.look += glm::vec2{f.dx * pixels.x, f.dy * pixels.y};
        if (!f.down)
        {
            if (const auto it = held_.find(f.id); it != held_.end())
            {
                input.HandleKeyUp(it->second);
                held_.erase(it);
            }
            if (stickFinger_ == f.id)
                stickFinger_.reset(), result.move = {0, 0};
            if (lookFinger_ == f.id)
                lookFinger_.reset();
        }
    }

    // 3. Draw: the stick (at rest where the thumb usually goes), then the buttons with labels.
    umbra::ui::Painter painter(out, fonts, atlas);
    const umbra::ui::ImageRef ring = images("ui/images/touch-ring.png"), disc = images("ui/images/touch-disc.png");
    const auto circle = [&](const umbra::ui::ImageRef &image, glm::vec2 c, float r, glm::vec4 color) {
        const umbra::ui::Rect rect{c.x - r, c.y - r, 2 * r, 2 * r};
        if (image.texture)
            painter.Image(rect, image.texture, color);
        else
            painter.Frame(rect, color, 3 * s); // no picture: a square still shows where to press
    };
    const glm::vec2 base = stickFinger_ ? stickBase_ : glm::vec2{170 * s, pixels.y - 170 * s};
    circle(ring, base, stickRadius, {1, 1, 1, stickFinger_ ? 0.55F : 0.3F});
    circle(disc, base + glm::vec2{result.move.x, -result.move.y} * stickRadius, 38 * s, {1, 1, 1, stickFinger_ ? 0.7F : 0.35F});
    const umbra::ui::Face *face = fonts.Find("strong") ? fonts.Find("strong") : fonts.Find("body");
    for (const Button &b : buttons)
    {
        const bool pressed = std::any_of(held_.begin(), held_.end(), [&](const auto &h) { return h.second == b.key; });
        circle(disc, b.center, b.radius, pressed ? glm::vec4{0.95F, 0.84F, 0.54F, 0.75F} : glm::vec4{0.08F, 0.08F, 0.1F, 0.45F});
        circle(ring, b.center, b.radius, {0.95F, 0.84F, 0.54F, 0.8F});
        if (face != nullptr)
        {
            const float h = static_cast<float>(face->lineHeight);
            painter.Text(*face, {b.center.x - b.radius, b.center.y - h * 0.5F, 2 * b.radius, h}, b.label, {1, 1, 1, 0.95F}, false, 0.5F);
        }
    }
    return result;
}
