#pragma once
#include <Input.hpp>
#include <ApplicationEvents.hpp>

namespace umbra::platform
{
    class EventPump
    {
    public:
        void PumpEvents(Input &input, ApplicationEvents &applicationEvents);
    };
}