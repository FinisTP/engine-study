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

TEST_CASE("A run's frame times summarize into average and nearest-rank percentiles (P9)")
{
    using umbra::core::Summarize;
    CHECK(Summarize({}).frames == 0);
    std::vector<double> samples;
    for (int i = 1; i <= 100; ++i)
        samples.push_back(static_cast<double>(i)); // 1..100 ms, shuffled order doesn't matter
    std::swap(samples[0], samples[99]);
    const auto stats = Summarize(samples);
    CHECK(stats.frames == 100);
    CHECK(stats.average == Approx(50.5));
    CHECK(stats.p50 == Approx(50.0));
    CHECK(stats.p95 == Approx(95.0));
    CHECK(stats.p99 == Approx(99.0));
    CHECK(stats.max == Approx(100.0));
    CHECK(Summarize({7.0}).p99 == Approx(7.0));
}
