#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include <umbra/core/FrameClock.hpp>

namespace
{
	using Clock = std::chrono::steady_clock;

	constexpr double comparisonMargin = 1.0e-9;
}

TEST_CASE("FrameClock samples controlled monotonic time")
{
	Clock::time_point currentTime{};
	umbra::core::FrameClock clock{ [&currentTime] { return currentTime; } };

	SECTION("Delta starts at zero")
	{
		REQUIRE(clock.GetDeltaTimeSeconds() == 0.0);
	}

	SECTION("First tick establishes the baseline")
	{
		currentTime += std::chrono::milliseconds{ 16 };
		clock.Tick();

		REQUIRE(clock.GetDeltaTimeSeconds() == 0.0);
	}

	SECTION("Normal elapsed time is reported in seconds")
	{
		clock.Tick();
		currentTime += std::chrono::milliseconds{ 16 };
		clock.Tick();

		REQUIRE(clock.GetDeltaTimeSeconds() == Catch::Approx(0.016).margin(comparisonMargin));
	}

	SECTION("Large deltas are clamped")
	{
		clock.Tick();
		currentTime += std::chrono::seconds{ 1 };
		clock.Tick();

		REQUIRE(clock.GetDeltaTimeSeconds() == Catch::Approx(0.25).margin(comparisonMargin));
	}

	SECTION("Each tick measures from the preceding tick")
	{
		clock.Tick();

		currentTime += std::chrono::milliseconds{ 10 };
		clock.Tick();
		REQUIRE(clock.GetDeltaTimeSeconds() == Catch::Approx(0.010).margin(comparisonMargin));

		currentTime += std::chrono::milliseconds{ 16 };
		clock.Tick();
		REQUIRE(clock.GetDeltaTimeSeconds() == Catch::Approx(0.016).margin(comparisonMargin));
	}

	SECTION("A clamped tick still records the actual current time")
	{
		clock.Tick();
		currentTime += std::chrono::seconds{ 1 };
		clock.Tick();
		REQUIRE(clock.GetDeltaTimeSeconds() == Catch::Approx(0.25).margin(comparisonMargin));

		currentTime += std::chrono::milliseconds{ 10 };
		clock.Tick();
		REQUIRE(clock.GetDeltaTimeSeconds() == Catch::Approx(0.010).margin(comparisonMargin));
	}

	SECTION("Reset makes the next tick establish a new baseline")
	{
		clock.Tick();
		currentTime += std::chrono::milliseconds{ 10 };
		clock.Tick();

		clock.ResetClock();
		currentTime += std::chrono::milliseconds{ 50 };
		clock.Tick();
		REQUIRE(clock.GetDeltaTimeSeconds() == 0.0);

		currentTime += std::chrono::milliseconds{ 7 };
		clock.Tick();
		REQUIRE(clock.GetDeltaTimeSeconds() == Catch::Approx(0.007).margin(comparisonMargin));
	}

	SECTION("A delta at the clamp boundary is unchanged")
	{
		clock.Tick();
		currentTime += std::chrono::milliseconds{ 250 };
		clock.Tick();

		REQUIRE(clock.GetDeltaTimeSeconds() == Catch::Approx(0.25).margin(comparisonMargin));
	}

	SECTION("A backward time source cannot produce a negative delta")
	{
		clock.Tick();
		currentTime += std::chrono::milliseconds{ 20 };
		clock.Tick();

		currentTime -= std::chrono::milliseconds{ 30 };
		clock.Tick();

		REQUIRE(clock.GetDeltaTimeSeconds() == 0.0);
	}
}
