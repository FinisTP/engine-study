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

        void ClearKeyStates() noexcept;

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
    };
}
