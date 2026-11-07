#pragma once

#include <PlayerController.hpp>
#include <Profiling.hpp>

#include <string>
#include <vector>

// Scripted performance run (roadmap P9): `Umbra --benchmark [seconds per area]`. The player walks
// in a slow circle through each reference area while every frame time is recorded; the first
// seconds of an area (loading, shader warm-up) are not counted. Rules in Benchmark.md.
class Benchmark final
{
public:
    // Budget on the reference PC (proposed 2026-11-08, ROADMAP O18; confirm with measurements).
    static constexpr double budgetAverageMs = 1000.0 / 60.0;
    static constexpr double budgetP99Ms = 1000.0 / 30.0;
    static constexpr float warmupSeconds = 2.0F;

    explicit Benchmark(float secondsPerArea);

    // The area to show now; "" once every area is done.
    [[nodiscard]] const std::string &Area() const;
    [[nodiscard]] bool Finished() const { return area_ >= areas_.size(); }
    // Records one frame; true when it moved on to the next area (the caller loads it).
    bool AddFrame(float deltaSeconds, double frameMs, double workMs, std::size_t entities);

    // What the scripted player does this frame: walk forward while turning.
    [[nodiscard]] umbra::gameplay::PlayerInput Input() const;
    [[nodiscard]] static float TurnRadiansPerSecond() { return 0.6F; }

    struct Machine final
    {
        std::string version, gpu;
        unsigned width = 0, height = 0;
        int cpuThreads = 0, ramMb = 0, peakMemoryMb = 0;
    };
    // The report as JSON; `passed` = every area within budget.
    [[nodiscard]] std::string Report(const Machine &machine, bool &passed) const;

private:
    struct AreaRun final
    {
        std::string scene;
        std::vector<double> frameMs, workMs;
        std::size_t entities = 0;
    };
    std::vector<AreaRun> areas_;
    std::size_t area_ = 0;
    float seconds_ = 0.0F; // in the current area
    float secondsPerArea_;
};
