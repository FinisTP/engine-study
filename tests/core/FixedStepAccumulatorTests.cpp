#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <umbra/core/FixedStepAccumulator.hpp>

namespace
{
	constexpr double comparisonMargin = 1.0e-9;
}

TEST_CASE("FixedStepAccumulator starts empty")
{
	umbra::core::FixedStepAccumulator accumulator{0.01};

	REQUIRE_FALSE(accumulator.MarkFixedStep());
	REQUIRE(accumulator.GetInterpolationFactor() == 0.0);
}

TEST_CASE("FixedStepAccumulator ignores non-positive delta time")
{
	umbra::core::FixedStepAccumulator accumulator{0.01};

	accumulator.AddDeltaTime(0.0);
	accumulator.AddDeltaTime(-0.01);

	REQUIRE_FALSE(accumulator.MarkFixedStep());
	REQUIRE(accumulator.GetInterpolationFactor() == 0.0);
}

TEST_CASE("FixedStepAccumulator reports partial-step interpolation")
{
	umbra::core::FixedStepAccumulator accumulator{0.01};

	accumulator.AddDeltaTime(0.004);

	REQUIRE_FALSE(accumulator.MarkFixedStep());
	REQUIRE(accumulator.GetInterpolationFactor() == Catch::Approx(0.4).margin(comparisonMargin));
}

TEST_CASE("FixedStepAccumulator consumes an exact fixed step")
{
	umbra::core::FixedStepAccumulator accumulator{0.01};

	accumulator.AddDeltaTime(0.01);

	REQUIRE(accumulator.MarkFixedStep());
	REQUIRE_FALSE(accumulator.MarkFixedStep());
	REQUIRE(accumulator.GetInterpolationFactor() == 0.0);
}

TEST_CASE("FixedStepAccumulator consumes multiple steps and retains the remainder")
{
	umbra::core::FixedStepAccumulator accumulator{0.01};

	accumulator.AddDeltaTime(0.025);

	REQUIRE(accumulator.MarkFixedStep());
	REQUIRE(accumulator.MarkFixedStep());
	REQUIRE_FALSE(accumulator.MarkFixedStep());
	REQUIRE(accumulator.GetInterpolationFactor() == Catch::Approx(0.5).margin(comparisonMargin));
}

TEST_CASE("FixedStepAccumulator reset clears pending time and interpolation")
{
	umbra::core::FixedStepAccumulator accumulator{0.01};

	accumulator.AddDeltaTime(0.015);
	REQUIRE(accumulator.MarkFixedStep());
	REQUIRE_FALSE(accumulator.MarkFixedStep());
	REQUIRE(accumulator.GetInterpolationFactor() == Catch::Approx(0.5).margin(comparisonMargin));

	accumulator.Reset();

	REQUIRE_FALSE(accumulator.MarkFixedStep());
	REQUIRE(accumulator.GetInterpolationFactor() == 0.0);
}
