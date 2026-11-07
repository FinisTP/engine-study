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
        case SDL_SCANCODE_F1:
            return InputKey::F1;
        case SDL_SCANCODE_E:
            return InputKey::E;
        case SDL_SCANCODE_R:
            return InputKey::R;
        case SDL_SCANCODE_F5:
            return InputKey::F5;
        case SDL_SCANCODE_F9:
            return InputKey::F9;
        case SDL_SCANCODE_Q:
            return InputKey::Q;
        case SDL_SCANCODE_F:
            return InputKey::F;
        case SDL_SCANCODE_1:
            return InputKey::Num1;
        case SDL_SCANCODE_2:
            return InputKey::Num2;
        case SDL_SCANCODE_3:
            return InputKey::Num3;
        case SDL_SCANCODE_4:
            return InputKey::Num4;
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
            if (rawHandler_)
                rawHandler_(&event);
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
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                applicationEvents.RequestWindowPixelSizeChanged();
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                applicationEvents.RequestWindowFocusGained();
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                applicationEvents.RequestWindowFocusLost();
                input.ClearKeyStates();
                break;
            case SDL_EVENT_WINDOW_MINIMIZED:
            case SDL_EVENT_WILL_ENTER_BACKGROUND:
            case SDL_EVENT_DID_ENTER_BACKGROUND:
                applicationEvents.RequestWindowSuspended();
                input.ClearKeyStates();
                break;
            case SDL_EVENT_WINDOW_RESTORED:
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
            case SDL_EVENT_MOUSE_MOTION:
                input.HandleMouseMotion(event.motion.xrel, event.motion.yrel);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                // SDL numbers buttons from 1 (left); Input from 0.
                input.HandleMouseButton(static_cast<int>(event.button.button) - 1, event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                break;
            default:
                break;
            }
        }
    }
}
