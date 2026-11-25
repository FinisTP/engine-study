#include <EventPump.hpp>
#include <SDL3/SDL.h>

#include <algorithm>

namespace
{
    umbra::platform::InputKey TranslateButton(Uint8 button)
    {
        using umbra::platform::InputKey;
        switch (button)
        {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            return InputKey::PadA;
        case SDL_GAMEPAD_BUTTON_EAST:
            return InputKey::PadB;
        case SDL_GAMEPAD_BUTTON_WEST:
            return InputKey::PadX;
        case SDL_GAMEPAD_BUTTON_NORTH:
            return InputKey::PadY;
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            return InputKey::PadUp;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            return InputKey::PadDown;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            return InputKey::PadLeft;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            return InputKey::PadRight;
        case SDL_GAMEPAD_BUTTON_START:
            return InputKey::PadStart;
        case SDL_GAMEPAD_BUTTON_BACK:
            return InputKey::PadBack;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            return InputKey::PadL1;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            return InputKey::PadR1;
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:
            return InputKey::PadL3;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
            return InputKey::PadR3;
        default:
            return InputKey::Unknown;
        }
    }

    umbra::platform::InputKey TranslateKeyCode(SDL_Scancode sdlKeyCode)
    {
        using umbra::platform::InputKey;

        switch (sdlKeyCode)
        {
        case SDL_SCANCODE_ESCAPE:
        case SDL_SCANCODE_AC_BACK: // the Android back button/gesture
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
        case SDL_SCANCODE_5:
            return InputKey::Num5;
        case SDL_SCANCODE_6:
            return InputKey::Num6;
        case SDL_SCANCODE_I:
            return InputKey::I;
        case SDL_SCANCODE_K:
            return InputKey::K;
        case SDL_SCANCODE_J:
            return InputKey::J;
        case SDL_SCANCODE_H:
            return InputKey::H;
        case SDL_SCANCODE_L:
            return InputKey::L;
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
            case SDL_EVENT_WILL_ENTER_BACKGROUND:
                applicationEvents.RequestEnteredBackground(); // Android/iOS: save now, release the surface
                [[fallthrough]];
            case SDL_EVENT_WINDOW_MINIMIZED:
            case SDL_EVENT_DID_ENTER_BACKGROUND:
                applicationEvents.RequestWindowSuspended();
                input.ClearKeyStates();
                break;
            case SDL_EVENT_DID_ENTER_FOREGROUND:
                applicationEvents.RequestEnteredForeground();
                [[fallthrough]];
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WILL_ENTER_FOREGROUND:
                applicationEvents.RequestWindowResumed();
                break;
            case SDL_EVENT_TERMINATING:
                applicationEvents.RequestTerminated();
                break;
            case SDL_EVENT_KEY_DOWN:
                input.HandleKeyDown(TranslateKeyCode(event.key.scancode), event.key.repeat);
                break;
            case SDL_EVENT_TEXT_INPUT: // only while the window has text input on (Window::SetTextInput)
                input.HandleText(event.text.text);
                break;
            case SDL_EVENT_KEY_UP:
                input.HandleKeyUp(TranslateKeyCode(event.key.scancode));
                break;
            // Gamepads: opened when plugged in; their buttons act like keys.
            case SDL_EVENT_GAMEPAD_ADDED:
                SDL_OpenGamepad(event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                input.HandleKeyDown(TranslateButton(event.gbutton.button), false);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                input.HandleKeyUp(TranslateButton(event.gbutton.button));
                break;
            case SDL_EVENT_MOUSE_MOTION:
                // A mouse made up from a finger only points (menus); the finger itself steers.
                if (event.motion.which == SDL_TOUCH_MOUSEID)
                    input.HandleMouseMotion(0.0F, 0.0F, event.motion.x, event.motion.y);
                else
                    input.HandleMouseMotion(event.motion.xrel, event.motion.yrel, event.motion.x, event.motion.y);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                input.HandleWheel(event.wheel.y);
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_MOTION:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_CANCELED:
                input.HandleFinger(event.tfinger.fingerID, event.tfinger.x, event.tfinger.y, event.tfinger.dx, event.tfinger.dy,
                                   event.type == SDL_EVENT_FINGER_DOWN ? 0 : event.type == SDL_EVENT_FINGER_MOTION ? 1 : 2);
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                // Sticks -32768..32767, triggers 0..32767 (SDL's axis numbers match Input::Axis).
                if (event.gaxis.axis <= SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
                    input.HandleAxis(event.gaxis.axis, static_cast<float>(event.gaxis.value) / 32767.0F);
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

namespace umbra::platform
{
    void Rumble(float strength, int milliseconds)
    {
        // Every open pad: both motors, scaled 0..1.
        int count = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&count);
        const auto power = static_cast<Uint16>(std::clamp(strength, 0.0F, 1.0F) * 65535.0F);
        for (int i = 0; ids && i < count; ++i)
            if (SDL_Gamepad *pad = SDL_GetGamepadFromID(ids[i]))
                SDL_RumbleGamepad(pad, power, power, static_cast<Uint32>(std::max(milliseconds, 0)));
        SDL_free(ids);
    }
}
