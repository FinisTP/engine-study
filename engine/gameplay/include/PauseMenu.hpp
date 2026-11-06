#pragma once

#include <array>
#include <string_view>

namespace umbra::gameplay
{
    enum class MenuAction
    {
        None,
        Resume,
        Restart,
        Quit
    };

    // Keyboard-driven pause menu state. The app reads keys and the renderer draws GetItems();
    // this class only decides what is selected and what a choice means.
    class PauseMenu final
    {
    public:
        static constexpr std::array<std::string_view, 3> items{"Resume", "Restart area", "Quit"};

        void Open() noexcept
        {
            open_ = true;
            selected_ = 0;
        }
        void Close() noexcept { open_ = false; }
        [[nodiscard]] bool IsOpen() const noexcept { return open_; }
        // Moves the highlight up (-1) or down (+1), wrapping around.
        void Move(int delta) noexcept;
        // Chooses the highlighted item. Resume and Restart close the menu.
        MenuAction Confirm() noexcept;
        [[nodiscard]] int GetSelected() const noexcept { return selected_; }

    private:
        bool open_ = false;
        int selected_ = 0;
    };
}
