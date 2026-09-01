#pragma once

#include <chrono>
#include <functional>

namespace umbra::core
{
	class FrameClock
	{
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;
        using TimeSource = std::function<TimePoint()>;
        using Duration = std::chrono::duration<double>;

	private:
        TimeSource timeSource_;
		TimePoint lastTickTime_;
		Duration deltaTime_;
		const double maxDeltaTimeSeconds_ = 0.25; // Maximum delta time to prevent large jumps in time
		bool isFirstTick_ = true;

	public:
		FrameClock();
        explicit FrameClock(TimeSource timeSource);
        
		double GetDeltaTimeSeconds() const noexcept;
		void Tick() noexcept;
		void ResetClock() noexcept;
	};
}