#pragma once

#include <Input.hpp>
#include <Painter.hpp>
#include <Ui.hpp>

#include <functional>
#include <map>
#include <optional>
#include <vector>

// On-screen controls for phones (P11, Mobile.md): a stick that appears where the left thumb goes
// down, camera drag on the right, and round buttons that press the same keys as a keyboard (so
// the rest of the game doesn't know about touch). Hidden until the first touch, and whenever a
// menu or a turn fight has the screen (taps reach menus as mouse clicks).
class TouchControls final
{
public:
    // What the game is doing: decides which buttons show.
    struct State final
    {
        bool exploring = false;     // walk, use, jump
        bool realtimeFight = false; // hotkeys 1-4 and flee
        bool menuOpen = false;      // a screen takes the input: no controls
    };
    struct Output final
    {
        glm::vec2 move{0.0F};       // -1..1, y = forward
        glm::vec2 look{0.0F};       // pixels dragged this frame
    };
    using ImageSource = std::function<umbra::ui::ImageRef(const std::string &)>;

    // Reads this frame's fingers, presses/releases keys in `input`, and draws the controls.
    Output Update(umbra::platform::Input &input, glm::vec2 pixels, const State &state, const umbra::ui::FontAtlas &fonts,
                  umbra::core::TextureHandle atlas, const ImageSource &images, umbra::core::UiDrawList &out);
    [[nodiscard]] bool Visible() const { return visible_; }

    struct Button final
    {
        const char *label;
        umbra::platform::InputKey key;
        glm::vec2 center; // pixels
        float radius;
    };
    // The buttons for this state and screen size (public for tests).
    [[nodiscard]] static std::vector<Button> Layout(glm::vec2 pixels, const State &state);

private:
    void ReleaseAll(umbra::platform::Input &input);

    bool visible_ = false;
    std::optional<std::uint64_t> stickFinger_, lookFinger_;
    glm::vec2 stickBase_{0.0F};
    std::map<std::uint64_t, umbra::platform::InputKey> held_; // finger -> the key it holds down
};
