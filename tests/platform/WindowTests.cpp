#include <catch2/catch_test_macros.hpp>

#include <umbra/platform/Window.hpp>

TEST_CASE("Window caches dimensions reported by a resize event")
{
	umbra::platform::Window window;

	window.Resize(1600, 900);

	REQUIRE(window.GetWidth() == 1600);
	REQUIRE(window.GetHeight() == 900);
}

TEST_CASE("Window ignores invalid dimensions reported by an event")
{
	umbra::platform::Window window;
	window.Resize(1280, 720);

	window.Resize(-1, 900);

	REQUIRE(window.GetWidth() == 1280);
	REQUIRE(window.GetHeight() == 720);
}
