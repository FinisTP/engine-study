#include <chrono>
#include <string>
#include <thread>

#include <umbra/core/Version.hpp>
#include <umbra/core/FrameClock.hpp>
#include <umbra/core/FixedStepAccumulator.hpp>
#include <umbra/diagnostics/Log.hpp>

#include <umbra/platform/Platform.hpp>
#include <umbra/platform/Input.hpp>
#include <umbra/platform/EventPump.hpp>
#include <umbra/platform/Window.hpp>
#include <umbra/platform/ApplicationEvents.hpp>
#include <umbra/diagnostics/CrashHandler.hpp>

int RunApplication()
{
	umbra::platform::Platform platform;

	umbra::diagnostics::Info(std::string{"Starting Umbra "} + std::string{umbra::core::Version()});

	{
		if (!platform.Initialize())
		{
			return 1;
		}

		{
			umbra::core::FrameClock clock;
			umbra::core::FixedStepAccumulator accumulator(1.0 / 60.0); // Fixed step of 1/60 seconds
			umbra::platform::Input input;
			umbra::platform::ApplicationEvents applicationEvents;
			umbra::platform::EventPump eventPump;

			{
				umbra::platform::Window window;
				if (!window.Create("Umbra", 1280, 720))
				{
					platform.Shutdown();
					return 1;
				}

				bool isSuspended = false;

				while (true)
				{
					input.BeginFrame();
					applicationEvents.BeginFrame();
					eventPump.PumpEvents(input, applicationEvents);

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::Quit) ||
						applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowClose) ||
						input.IsKeyPressed(umbra::platform::InputKey::Escape))
					{
						break;
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowResize))
					{
						const int newWidth = applicationEvents.GetWindowWidth();
						const int newHeight = applicationEvents.GetWindowHeight();
						window.UpdateSizeFromEvent(newWidth, newHeight);
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowFocusGained))
					{
						// Handle window focus gained event
						umbra::diagnostics::Info("Window focus gained.");
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowFocusLost))
					{
						// Handle window focus lost event
						umbra::diagnostics::Info("Window focus lost.");
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowSuspended))
					{
						// Handle window suspended event
						umbra::diagnostics::Info("Window suspended.");
						isSuspended = true;
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::WindowResumed))
					{
						// Handle window resumed event
						umbra::diagnostics::Info("Window resumed.");
						clock.ResetClock();
						accumulator.Reset();
						isSuspended = false;
					}

					if (applicationEvents.IsEventRequested(umbra::platform::ApplicationEventType::Terminated))
					{
						// Handle application termination event
						umbra::diagnostics::Info("Application terminated.");
						break;
					}

					if (isSuspended)
					{
						// If the application is suspended, skip the update and rendering loop
						std::this_thread::sleep_for(std::chrono::milliseconds{100});
						continue;
					}

					clock.Tick();
					const double deltaTime = clock.GetDeltaTimeSeconds();
					accumulator.AddDeltaTime(deltaTime);

					while (accumulator.MarkFixedStep())
					{
						// Perform fixed-step updates here (like physics)
					}

					const double interpolationFactor = accumulator.GetInterpolationFactor();
					// rendering loop

					std::this_thread::sleep_for(std::chrono::milliseconds{1});
				}
			}
		}
	}

	platform.Shutdown();
	return 0;
}

int main()
{
	umbra::diagnostics::Initialize();
	umbra::diagnostics::InstallCrashHandlers();

	const int result = RunApplication();

	umbra::diagnostics::UninstallCrashHandlers();
	umbra::diagnostics::Shutdown();
	return result;
}
