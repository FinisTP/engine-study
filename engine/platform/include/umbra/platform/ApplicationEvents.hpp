#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace umbra::platform
{
    enum class ApplicationEventType : std::uint8_t
    {
        Quit,
        WindowClose,
        WindowResize,
        WindowFocusGained,
        WindowFocusLost,
        Count
    };

    class ApplicationEvents
    {
    private:
        static constexpr std::size_t eventCount_ = static_cast<std::size_t>(ApplicationEventType::Count);
        std::array<bool, eventCount_> requestedEvents_{};
        int windowWidth_ = 0;
        int windowHeight_ = 0;
        void Request(ApplicationEventType eventType) noexcept;

    public:
        void RequestQuit() noexcept;
        void RequestWindowClose() noexcept;
        void RequestWindowResize(int width, int height) noexcept;
        void RequestWindowFocusGained() noexcept;
        void RequestWindowFocusLost() noexcept;

        void BeginFrame() noexcept;
        [[nodiscard]] bool IsEventRequested(ApplicationEventType eventType) const noexcept;
        [[nodiscard]] int GetWindowWidth() const noexcept { return windowWidth_; }
        [[nodiscard]] int GetWindowHeight() const noexcept { return windowHeight_; }
    };
}
