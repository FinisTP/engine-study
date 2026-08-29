#include <chrono>
#include <string>
#include <thread>

#include <umbra/core/Version.hpp>
#include <umbra/core/FrameClock.hpp>
#include <umbra/core/FixedStepAccumulator.hpp>
#include <umbra/diagnostics/Log.hpp>
#include <umbra/platform/Platform.hpp>

int main()
{
	umbra::diagnostics::Initialize();

	umbra::diagnostics::Info(std::string{"Starting Umbra "} + std::string{umbra::core::Version()});

	if (!umbra::platform::Initialize())
	{
		umbra::diagnostics::Shutdown();
		return 1;
	}

	{
		umbra::platform::Window window;

		if (!window.Create("Umbra", 1280, 720))
		{
			umbra::platform::Shutdown();
			umbra::diagnostics::Shutdown();
			return 1;
		}

		umbra::core::FrameClock clock;
		umbra::core::FixedStepAccumulator accumulator(1.0 / 60.0); // Fixed step of 1/60 seconds

		while (!window.PollCloseRequested())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds{1});

			clock.Tick();
			const double deltaTime = clock.GetDeltaTimeSeconds();
			accumulator.AddDeltaTime(deltaTime);

			while (accumulator.MarkFixedStep())
			{
				// Perform fixed-step updates here (like physics)
			}

			const double interpolationFactor = accumulator.GetInterpolationFactor();
			// rendering loop
		}
	}

	umbra::platform::Shutdown();
	umbra::diagnostics::Shutdown();

	return 0;
}