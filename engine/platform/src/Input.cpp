#include <Input.hpp>

namespace umbra::platform
{
    void Input::HandleKeyDown(InputKey key, bool isRepeat) noexcept
    {
        // Unmapped keys arrive as Unknown and are ignored.
        if (!IsValidKey(key))
        {
            return;
        }

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
    }

    void Input::HandleMouseMotion(float dx, float dy) noexcept
    {
        mouseDeltaX_ += dx;
        mouseDeltaY_ += dy;
    }

    void Input::HandleMouseButton(int button, bool down) noexcept
    {
        if (button < 0 || button >= static_cast<int>(mouseHeld_.size()))
        {
            return;
        }
        const auto index = static_cast<std::size_t>(button);
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
    }
}
