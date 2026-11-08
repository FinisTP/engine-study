#include <PauseMenu.hpp>

namespace umbra::gameplay
{
    void PauseMenu::Move(int delta) noexcept
    {
        if (!open_)
            return;
        const int count = static_cast<int>(items.size());
        selected_ = ((selected_ + delta) % count + count) % count; // wrap, also for negative delta
    }

    MenuAction PauseMenu::Confirm() noexcept
    {
        if (!open_)
            return MenuAction::None;
        const MenuAction actions[] = {MenuAction::Resume, MenuAction::Restart, MenuAction::Quit};
        const MenuAction action = actions[selected_];
        if (action != MenuAction::Quit)
            open_ = false;
        return action;
    }
}
