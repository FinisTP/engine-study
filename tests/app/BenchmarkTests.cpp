#include <Benchmark.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("The benchmark visits the three areas, skips the warm-up and judges the budget (P9)")
{
    Benchmark run(1.0F);
    REQUIRE(run.Area() == "scenes/settlement.scene.json");
    CHECK(run.Input().move.y == 1.0F);

    // 1. Each area: 2 s warm-up + 1 s counted, at 10 ms frames; the second area has a slow frame.
    const char *next[] = {"scenes/dungeon.scene.json", "scenes/arena.scene.json", ""};
    for (int area = 0; area < 3; ++area)
    {
        bool moved = false;
        for (int frame = 0; !moved; ++frame)
        {
            const double ms = area == 1 && frame == 250 ? 50.0 : 10.0;
            moved = run.AddFrame(0.01F, ms, 5.0, 42);
        }
        CHECK(run.Area() == next[area]);
    }
    CHECK(run.Finished());

    // 2. The report: frames after the warm-up only, one hitch is under 1 % -> still within budget.
    bool passed = false;
    const std::string report = run.Report({"0.9", "Test GPU", 1920, 1080, 8, 16000, 300}, passed);
    CHECK(passed);
    CHECK(report.find("\"gpu\": \"Test GPU\"") != std::string::npos);
    CHECK(report.find("\"peakMemoryMb\": 300") != std::string::npos);
    CHECK(report.find("\"scene\": \"scenes/dungeon.scene.json\", \"frames\": 10") != std::string::npos);
    CHECK(report.find("\"maxMs\": 50.00") != std::string::npos);

    // 3. A slow run is over budget.
    Benchmark slow(1.0F);
    while (!slow.Finished())
        slow.AddFrame(0.05F, 50.0, 40.0, 1);
    (void)slow.Report({}, passed);
    CHECK_FALSE(passed);
}
