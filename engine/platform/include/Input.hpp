#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace umbra::platform
{
    enum class InputKey : std::uint8_t
    {
        Unknown,
        Escape,
        Space,
        Enter,
        Tab,
        Backspace,
        W,
        A,
        S,
        D,
        Up,
        Down,
        Left,
        Right,
        LeftShift,
        RightShift,
        LeftCtrl,
        RightCtrl,
        E,
        R,
        F1,
        F5,
        F9,
        Q,
        F,
        Num1,
        Num2,
        Num3,
        Num4,
        I, // inventory
        K, // skills
        J, // journal (quests)
        H, // hotkeys
        // Gamepad buttons (SDL gamepad, Xbox names): they act like keys.
        PadA,
        PadB,
        PadX,
        PadY,
        PadUp,
        PadDown,
        PadLeft,
        PadRight,
        PadStart,
        PadBack,
        Count
    };

    enum class InputKeyState : std::uint8_t
    {
        Unknown = 0,
        Pressed = 1 << 0,
        Held = 1 << 1,
        Released = 1 << 2
    };

    [[nodiscard]] constexpr InputKeyState operator|(InputKeyState lhs, InputKeyState rhs) noexcept
    {
        return static_cast<InputKeyState>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
    }

    [[nodiscard]] constexpr InputKeyState operator&(InputKeyState lhs, InputKeyState rhs) noexcept
    {
        return static_cast<InputKeyState>(static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
    }

    class Input final
    {
    public:
        void HandleKeyDown(InputKey key, bool isRepeat) noexcept;
        void HandleKeyUp(InputKey key) noexcept;
        void BeginFrame() noexcept;

        [[nodiscard]] InputKeyState GetKeyState(InputKey key) const noexcept;
        [[nodiscard]] bool IsKeyPressed(InputKey key) const noexcept;
        [[nodiscard]] bool IsKeyHeld(InputKey key) const noexcept;
        [[nodiscard]] bool IsKeyReleased(InputKey key) const noexcept;

        // Clears keys, mouse buttons and mouse motion (focus loss, suspend).
        void ClearKeyStates() noexcept;

        // Mouse motion in pixels since the last BeginFrame (raw motion while the mouse is captured),
        // and where the pointer is in the window (window coordinates, not render pixels).
        void HandleMouseMotion(float dx, float dy, float x = -1.0F, float y = -1.0F) noexcept;
        [[nodiscard]] float GetMouseX() const noexcept { return mouseX_; }
        [[nodiscard]] float GetMouseY() const noexcept { return mouseY_; }
        [[nodiscard]] bool MouseMoved() const noexcept { return mouseDeltaX_ != 0.0F || mouseDeltaY_ != 0.0F; }
        // button: 0 = left, 1 = right, 2 = middle.
        void HandleMouseButton(int button, bool down) noexcept;
        [[nodiscard]] float GetMouseDeltaX() const noexcept { return mouseDeltaX_; }
        [[nodiscard]] float GetMouseDeltaY() const noexcept { return mouseDeltaY_; }
        [[nodiscard]] bool IsMouseButtonPressed(int button) const noexcept;
        [[nodiscard]] bool IsMouseButtonHeld(int button) const noexcept;

    private:
        static constexpr std::size_t keyCount_ = static_cast<std::size_t>(InputKey::Count);

        [[nodiscard]] static constexpr bool IsValidKey(InputKey key) noexcept
        {
            return key != InputKey::Unknown && static_cast<std::size_t>(key) < keyCount_;
        }

        [[nodiscard]] static constexpr std::size_t ToIndex(InputKey key) noexcept
        {
            return static_cast<std::size_t>(key);
        }

        std::array<bool, keyCount_> pressedKeys_{};
        std::array<bool, keyCount_> releasedKeys_{};
        std::array<bool, keyCount_> heldKeys_{};
        std::array<bool, 3> mousePressed_{};
        std::array<bool, 3> mouseHeld_{};
        float mouseDeltaX_ = 0.0F;
        float mouseDeltaY_ = 0.0F;
        float mouseX_ = -1.0F, mouseY_ = -1.0F;
    };
}
