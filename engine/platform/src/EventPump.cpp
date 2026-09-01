#include <EventPump.hpp>
#include <SDL3/SDL.h>

namespace
{
    umbra::platform::InputKey TranslateKeyCode(SDL_Scancode sdlKeyCode)
    {
        using umbra::platform::InputKey;

        switch (sdlKeyCode)
        {
        case SDL_SCANCODE_ESCAPE:
            return InputKey::Escape;
        case SDL_SCANCODE_SPACE:
            return InputKey::Space;
        case SDL_SCANCODE_RETURN:
            return InputKey::Enter;
        case SDL_SCANCODE_TAB:
            return InputKey::Tab;
        case SDL_SCANCODE_BACKSPACE:
            return InputKey::Backspace;
        case SDL_SCANCODE_W:
            return InputKey::W;
        case SDL_SCANCODE_A:
            return InputKey::A;
        case SDL_SCANCODE_S:
            return InputKey::S;
        case SDL_SCANCODE_D:
            return InputKey::D;
        case SDL_SCANCODE_UP:
            return InputKey::Up;
        case SDL_SCANCODE_DOWN:
            return InputKey::Down;
        case SDL_SCANCODE_LEFT:
            return InputKey::Left;
        case SDL_SCANCODE_RIGHT:
            return InputKey::Right;
        case SDL_SCANCODE_LSHIFT:
            return InputKey::LeftShift;
        case SDL_SCANCODE_RSHIFT:
            return InputKey::RightShift;
        case SDL_SCANCODE_LCTRL:
            return InputKey::LeftCtrl;
        case SDL_SCANCODE_RCTRL:
            return InputKey::RightCtrl;
        default:
            return InputKey::Unknown;
        }
    }

}

namespace umbra::platform
{
    void EventPump::PumpEvents(Input &input, ApplicationEvents &applicationEvents)
    {
        SDL_Event event{};
        // Implementation for pumping events
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                applicationEvents.RequestQuit();
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                applicationEvents.RequestWindowClose();
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                applicationEvents.RequestWindowResize(event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                applicationEvents.RequestWindowFocusGained();
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                applicationEvents.RequestWindowFocusLost();
                input.ClearKeyStates();
                break;
            case SDL_EVENT_WILL_ENTER_BACKGROUND:
            case SDL_EVENT_DID_ENTER_BACKGROUND:
                applicationEvents.RequestWindowSuspended();
                input.ClearKeyStates();
                break;
            case SDL_EVENT_WILL_ENTER_FOREGROUND:
            case SDL_EVENT_DID_ENTER_FOREGROUND:
                applicationEvents.RequestWindowResumed();
                break;
            case SDL_EVENT_TERMINATING:
                applicationEvents.RequestTerminated();
                break;
            case SDL_EVENT_KEY_DOWN:
                input.HandleKeyDown(TranslateKeyCode(event.key.scancode), event.key.repeat);
                break;
            case SDL_EVENT_KEY_UP:
                input.HandleKeyUp(TranslateKeyCode(event.key.scancode));
                break;
            default:
                break;
            }
        }
    }
}
