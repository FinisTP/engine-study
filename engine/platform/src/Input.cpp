#include <Input.hpp>

#include <algorithm>
#include <cmath>

namespace umbra::platform
{
    void Input::HandleKeyDown(InputKey key, bool isRepeat) noexcept
    {
        // Unmapped keys arrive as Unknown and are ignored.
        if (!IsValidKey(key))
        {
            return;
        }

        padLast_ = IsPadKey(key);
        if (key == InputKey::Backspace)
            text_ += '\b'; // held keys repeat: each repeat erases one more letter
        const std::size_t index = ToIndex(key);
        if (!isRepeat && !heldKeys_[index])
        {
            pressedKeys_[index] = true;
        }

        heldKeys_[index] = true;
    }

    void Input::HandleKeyUp(InputKey key) noexcept
    {
        if (!IsValidKey(key))
        {
            return;
        }

        const std::size_t index = ToIndex(key);
        heldKeys_[index] = false;
        releasedKeys_[index] = true;
    }

    void Input::BeginFrame() noexcept
    {
        pressedKeys_.fill(false);
        releasedKeys_.fill(false);
        mousePressed_.fill(false);
        mouseDeltaX_ = 0.0F;
        mouseDeltaY_ = 0.0F;
        wheel_ = 0.0F;
        text_.clear();
        // Lifted fingers were seen for one frame; the others start a new frame.
        std::erase_if(fingers_, [](const Finger &f) { return !f.down; });
        for (Finger &f : fingers_)
            f.dx = f.dy = 0.0F, f.pressed = false;
    }

    void Input::HandleFinger(std::uint64_t id, float x, float y, float dx, float dy, int phase)
    {
        touchSeen_ = true;
        auto it = std::find_if(fingers_.begin(), fingers_.end(), [&](const Finger &f) { return f.id == id && f.down; });
        if (phase == 0 || it == fingers_.end())
        {
            if (phase != 0)
                return; // a finger we never saw go down (e.g. down before a resume)
            fingers_.push_back({id, x, y, x, y, 0.0F, 0.0F, true, true});
            return;
        }
        it->x = x, it->y = y;
        it->dx += dx, it->dy += dy;
        it->down = phase != 2;
    }

    void Input::HandleAxis(int axis, float value) noexcept
    {
        // A small dead zone: worn sticks rest a little off center.
        constexpr float deadZone = 0.2F;
        if (axis >= 0 && axis < 6)
            axes_[static_cast<std::size_t>(axis)] = std::abs(value) < deadZone ? 0.0F : (value - std::copysign(deadZone, value)) / (1.0F - deadZone);
        if (std::abs(value) > 0.5F)
            padLast_ = true;
    }

    void Input::HandleMouseMotion(float dx, float dy, float x, float y) noexcept
    {
        mouseDeltaX_ += dx;
        mouseDeltaY_ += dy;
        if (std::abs(dx) + std::abs(dy) > 2.0F)
            padLast_ = false;
        if (x >= 0.0F)
            mouseX_ = x, mouseY_ = y;
    }

    void Input::HandleMouseButton(int button, bool down) noexcept
    {
        if (button < 0 || button >= static_cast<int>(mouseHeld_.size()))
        {
            return;
        }
        const auto index = static_cast<std::size_t>(button);
        if (down)
            padLast_ = false;
        if (down && !mouseHeld_[index])
        {
            mousePressed_[index] = true;
        }
        mouseHeld_[index] = down;
    }

    bool Input::IsMouseButtonPressed(int button) const noexcept
    {
        return button >= 0 && button < static_cast<int>(mousePressed_.size()) && mousePressed_[static_cast<std::size_t>(button)];
    }

    bool Input::IsMouseButtonHeld(int button) const noexcept
    {
        return button >= 0 && button < static_cast<int>(mouseHeld_.size()) && mouseHeld_[static_cast<std::size_t>(button)];
    }

    InputKeyState Input::GetKeyState(InputKey key) const noexcept
    {
        if (!IsValidKey(key))
        {
            return InputKeyState::Unknown;
        }

        const std::size_t index = ToIndex(key);
        InputKeyState state = InputKeyState::Unknown;
        if (pressedKeys_[index])
        {
            state = state | InputKeyState::Pressed;
        }
        if (heldKeys_[index])
        {
            state = state | InputKeyState::Held;
        }
        if (releasedKeys_[index])
        {
            state = state | InputKeyState::Released;
        }

        return state;
    }

    bool Input::IsKeyPressed(InputKey key) const noexcept
    {
        return (GetKeyState(key) & InputKeyState::Pressed) == InputKeyState::Pressed;
    }

    bool Input::IsKeyHeld(InputKey key) const noexcept
    {
        return (GetKeyState(key) & InputKeyState::Held) == InputKeyState::Held;
    }

    bool Input::IsKeyReleased(InputKey key) const noexcept
    {
        return (GetKeyState(key) & InputKeyState::Released) == InputKeyState::Released;
    }

    void Input::ClearKeyStates() noexcept
    {
        pressedKeys_.fill(false);
        releasedKeys_.fill(false);
        heldKeys_.fill(false);
        mousePressed_.fill(false);
        mouseHeld_.fill(false);
        mouseDeltaX_ = 0.0F;
        mouseDeltaY_ = 0.0F;
        wheel_ = 0.0F;
        fingers_.clear();
        axes_.fill(0.0F);
    }
}
