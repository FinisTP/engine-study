#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Profiling.hpp>

using Catch::Approx;
using umbra::core::RollingTimer;

TEST_CASE("RollingTimer averages and maxes only the samples it has")
{
    RollingTimer timer;
    REQUIRE(timer.Average() == 0.0);
    REQUIRE(timer.Max() == 0.0);
    timer.Add(2.0);
    timer.Add(4.0);
    REQUIRE(timer.Average() == Approx(3.0));
    REQUIRE(timer.Max() == Approx(4.0));
}

TEST_CASE("RollingTimer forgets samples older than its window")
{
    RollingTimer timer;
    timer.Add(100.0); // a hitch
    for (std::size_t i = 0; i < RollingTimer::window; ++i)
        timer.Add(1.0);
    REQUIRE(timer.Max() == Approx(1.0));
    REQUIRE(timer.Average() == Approx(1.0));
}

TEST_CASE("Stopwatch laps restart the count")
{
    umbra::core::Stopwatch watch;
    const double first = watch.Lap();
    REQUIRE(first >= 0.0);
    REQUIRE(watch.Milliseconds() >= 0.0);
}
