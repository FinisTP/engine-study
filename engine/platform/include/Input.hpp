#pragma once

#include <array>
#include <vector>
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
        L, // log (the town's chronicle)
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

    // One finger on a touch screen; positions are 0..1 across the window (SDL's normalized touch).
    struct Finger final
    {
        std::uint64_t id = 0;
        float x = 0.0F, y = 0.0F;   // now
        float startX = 0.0F, startY = 0.0F; // where it went down
        float dx = 0.0F, dy = 0.0F; // moved since the last BeginFrame
        bool down = true;           // false: lifted this frame (removed next frame)
        bool pressed = true;        // went down this frame
    };

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
        // Wheel steps since the last BeginFrame (positive: away from the user).
        void HandleWheel(float steps) noexcept { wheel_ += steps; }
        [[nodiscard]] float Wheel() const noexcept { return wheel_; }
        [[nodiscard]] bool IsMouseButtonPressed(int button) const noexcept;
        [[nodiscard]] bool IsMouseButtonHeld(int button) const noexcept;

        // Touch (P11): fingers on the screen, oldest first; lifted ones stay for one frame.
        void HandleFinger(std::uint64_t id, float x, float y, float dx, float dy, int phase); // phase: 0 down, 1 move, 2 up
        [[nodiscard]] const std::vector<Finger> &Fingers() const noexcept { return fingers_; }
        [[nodiscard]] bool TouchSeen() const noexcept { return touchSeen_; }

        // Gamepad sticks, -1..1 with a dead zone: 0 left x, 1 left y, 2 right x, 3 right y (y down).
        void HandleAxis(int axis, float value) noexcept;
        [[nodiscard]] float Axis(int axis) const noexcept { return axis >= 0 && axis < 4 ? axes_[static_cast<std::size_t>(axis)] : 0.0F; }

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
        float wheel_ = 0.0F;
        float mouseX_ = -1.0F, mouseY_ = -1.0F;
        std::vector<Finger> fingers_;
        bool touchSeen_ = false;
        std::array<float, 4> axes_{};
    };
}
