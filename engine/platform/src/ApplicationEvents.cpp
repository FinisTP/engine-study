#include <ApplicationEvents.hpp>

namespace umbra::platform
{
    void ApplicationEvents::Request(ApplicationEventType eventType) noexcept
    {
        requestedEvents_[static_cast<std::size_t>(eventType)] = true;
    }

    void ApplicationEvents::RequestQuit() noexcept
    {
        Request(ApplicationEventType::Quit);
    }

    void ApplicationEvents::RequestWindowClose() noexcept
    {
        Request(ApplicationEventType::WindowClose);
    }

    void ApplicationEvents::RequestWindowResize(int width, int height) noexcept
    {
        Request(ApplicationEventType::WindowResize);
        windowWidth_ = width;
        windowHeight_ = height;
    }

    void ApplicationEvents::RequestWindowFocusGained() noexcept
    {
        Request(ApplicationEventType::WindowFocusGained);
    }

    void ApplicationEvents::RequestWindowFocusLost() noexcept
    {
        Request(ApplicationEventType::WindowFocusLost);
    }

    void ApplicationEvents::RequestWindowSuspended() noexcept
    {
        Request(ApplicationEventType::WindowSuspended);
    }

    void ApplicationEvents::RequestWindowResumed() noexcept
    {
        Request(ApplicationEventType::WindowResumed);
    }

    void ApplicationEvents::RequestTerminated() noexcept
    {
        Request(ApplicationEventType::Terminated);
    }

    void ApplicationEvents::BeginFrame() noexcept
    {
        requestedEvents_.fill(false);
    }

    bool ApplicationEvents::IsEventRequested(ApplicationEventType eventType) const noexcept
    {
        const std::size_t index = static_cast<std::size_t>(eventType);
        if (index >= eventCount_)
        {
            return false;
        }

        return requestedEvents_[index];
    }
}
