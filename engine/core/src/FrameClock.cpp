#include <FrameClock.hpp>
#include <algorithm>
#include <utility>

namespace umbra::core
{

    FrameClock::FrameClock() : FrameClock([]() { return Clock::now(); }) {}

    FrameClock::FrameClock(TimeSource timeSource) : timeSource_(std::move(timeSource))
    {
        // Initialize the clock by setting the last tick and reset times to the current time
        lastTickTime_ = timeSource_();
        deltaTime_ = Duration::zero();
    }

    void FrameClock::Tick() noexcept
    {
        // first tick returns 0, subsequent tick returns monotonic time since last tick
        auto currentTime = timeSource_();
        if (isFirstTick_)
        {
            deltaTime_ = Duration::zero();
            isFirstTick_ = false;
        }
        else
        {
            deltaTime_ = currentTime - lastTickTime_;
        }
        deltaTime_ = std::min(deltaTime_, Duration(maxDeltaTimeSeconds_));
        deltaTime_ = std::max(deltaTime_, Duration::zero());
        lastTickTime_ = currentTime;
    }

    void FrameClock::ResetClock() noexcept
    {
        lastTickTime_ = timeSource_();
        deltaTime_ = Duration::zero();
        isFirstTick_ = true;
    }

    double FrameClock::GetDeltaTimeSeconds() const noexcept
    {
        return deltaTime_.count();
    }
}