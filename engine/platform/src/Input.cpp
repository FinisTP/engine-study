#include <Input.hpp>
#include <Log.hpp>
#include <string>

namespace umbra::platform
{
    void Input::HandleKeyDown(InputKey key, bool isRepeat) noexcept
    {
        if (!IsValidKey(key))
        {
            diagnostics::Error("Invalid key pressed.");
            return;
        }

        diagnostics::Info("Key pressed: " + std::to_string(static_cast<std::uint8_t>(key)) + (isRepeat ? " (repeat)" : ""));

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
            diagnostics::Error("Invalid key released.");
            return;
        }

        diagnostics::Info("Key released: " + std::to_string(static_cast<std::uint8_t>(key)));

        const std::size_t index = ToIndex(key);
        heldKeys_[index] = false;
        releasedKeys_[index] = true;
    }

    void Input::BeginFrame() noexcept
    {
        pressedKeys_.fill(false);
        releasedKeys_.fill(false);
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
    }
}
