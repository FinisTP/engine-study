#pragma once
#include <umbra/platform/Input.hpp>
#include <umbra/platform/ApplicationEvents.hpp>

namespace umbra::platform
{
    class EventPump
    {
    public:
        void PumpEvents(Input &input, ApplicationEvents &applicationEvents);
    };
}