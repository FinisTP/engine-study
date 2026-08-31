#include <catch2/catch_test_macros.hpp>

#include <type_traits>

#include <umbra/platform/Platform.hpp>

static_assert(std::is_destructible_v<umbra::platform::Platform>);
static_assert(std::is_nothrow_destructible_v<umbra::platform::Platform>);
static_assert(!std::is_copy_constructible_v<umbra::platform::Platform>);
static_assert(!std::is_copy_assignable_v<umbra::platform::Platform>);
static_assert(!std::is_move_constructible_v<umbra::platform::Platform>);
static_assert(!std::is_move_assignable_v<umbra::platform::Platform>);

TEST_CASE("Platform shutdown is safe before initialization and when repeated")
{
	umbra::platform::Platform platform;

	REQUIRE_NOTHROW(platform.Shutdown());
	REQUIRE_NOTHROW(platform.Shutdown());
}
