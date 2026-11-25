#pragma once
#include <Input.hpp>
#include <ApplicationEvents.hpp>

#include <functional>

namespace umbra::platform
{
    class EventPump
    {
    public:
        void PumpEvents(Input &input, ApplicationEvents &applicationEvents);
        // Also hands every raw SDL_Event (as const void *) to `handler`, e.g. for Dear ImGui's
        // SDL backend in the editor. The game doesn't use it.
        void SetRawEventHandler(std::function<void(const void *)> handler) { rawHandler_ = std::move(handler); }

    private:
        std::function<void(const void *)> rawHandler_;
    };

    // Shakes every open gamepad for `milliseconds` at `strength` 0..1 (no pad: nothing happens).
    void Rumble(float strength, int milliseconds);
}
