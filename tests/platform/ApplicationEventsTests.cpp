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
