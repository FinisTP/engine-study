#include <catch2/catch_test_macros.hpp>

#include <umbra/platform/ApplicationEvents.hpp>

TEST_CASE("Application events are frame-local")
{
	umbra::platform::ApplicationEvents events;

	events.RequestQuit();
	events.RequestWindowFocusLost();

	REQUIRE(events.IsEventRequested(umbra::platform::ApplicationEventType::Quit));
	REQUIRE(events.IsEventRequested(umbra::platform::ApplicationEventType::WindowFocusLost));

	events.BeginFrame();

	REQUIRE_FALSE(events.IsEventRequested(umbra::platform::ApplicationEventType::Quit));
	REQUIRE_FALSE(events.IsEventRequested(umbra::platform::ApplicationEventType::WindowFocusLost));
}

TEST_CASE("Application resize events retain their dimensions")
{
	umbra::platform::ApplicationEvents events;

	events.RequestWindowResize(1600, 900);

	REQUIRE(events.IsEventRequested(umbra::platform::ApplicationEventType::WindowResize));
	REQUIRE(events.GetWindowWidth() == 1600);
	REQUIRE(events.GetWindowHeight() == 900);
}

TEST_CASE("Application suspension events are frame-local")
{
	umbra::platform::ApplicationEvents events;

	events.RequestWindowSuspended();
	REQUIRE(events.IsEventRequested(umbra::platform::ApplicationEventType::WindowSuspended));

	events.BeginFrame();
	REQUIRE_FALSE(events.IsEventRequested(umbra::platform::ApplicationEventType::WindowSuspended));
}

TEST_CASE("Application resume events are frame-local")
{
	umbra::platform::ApplicationEvents events;

	events.RequestWindowResumed();
	REQUIRE(events.IsEventRequested(umbra::platform::ApplicationEventType::WindowResumed));

	events.BeginFrame();
	REQUIRE_FALSE(events.IsEventRequested(umbra::platform::ApplicationEventType::WindowResumed));
}

TEST_CASE("Application termination events are frame-local")
{
	umbra::platform::ApplicationEvents events;

	events.RequestTerminated();
	REQUIRE(events.IsEventRequested(umbra::platform::ApplicationEventType::Terminated));

	events.BeginFrame();
	REQUIRE_FALSE(events.IsEventRequested(umbra::platform::ApplicationEventType::Terminated));
}
