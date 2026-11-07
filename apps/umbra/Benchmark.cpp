#include "Benchmark.hpp"

#include <cstdio>

Benchmark::Benchmark(float secondsPerArea) : secondsPerArea_(secondsPerArea)
{
    for (const char *scene : {"scenes/settlement.scene.json", "scenes/dungeon.scene.json", "scenes/arena.scene.json"})
        areas_.push_back({scene, {}, {}, 0});
}

const std::string &Benchmark::Area() const
{
    static const std::string none;
    return Finished() ? none : areas_[area_].scene;
}

bool Benchmark::AddFrame(float deltaSeconds, double frameMs, double workMs, std::size_t entities)
{
    if (Finished())
        return false;
    // 1. Count the frame unless the area is still warming up.
    AreaRun &area = areas_[area_];
    seconds_ += deltaSeconds;
    if (seconds_ > warmupSeconds)
    {
        area.frameMs.push_back(frameMs);
        area.workMs.push_back(workMs);
        area.entities = entities;
    }
    // 2. Next area after its time.
    if (seconds_ < warmupSeconds + secondsPerArea_)
        return false;
    ++area_;
    seconds_ = 0.0F;
    return true;
}

umbra::gameplay::PlayerInput Benchmark::Input() const
{
    umbra::gameplay::PlayerInput input;
    input.move = {0.0F, 1.0F}; // forward; the camera turns, so the path is a circle
    return input;
}

std::string Benchmark::Report(const Machine &m, bool &passed) const
{
    // Plain text JSON: the game doesn't link a JSON library for this.
    char line[512];
    std::string out = "{\n";
    std::snprintf(line, sizeof(line),
                  "  \"version\": \"%s\",\n  \"gpu\": \"%s\",\n  \"resolution\": [%u, %u],\n  \"cpuThreads\": %d,\n  \"ramMb\": %d,\n  \"peakMemoryMb\": %d,\n"
                  "  \"secondsPerArea\": %.0f,\n  \"budget\": {\"averageMs\": %.2f, \"p99Ms\": %.2f},\n  \"areas\": [\n",
                  m.version.c_str(), m.gpu.c_str(), m.width, m.height, m.cpuThreads, m.ramMb, m.peakMemoryMb, secondsPerArea_, budgetAverageMs, budgetP99Ms);
    out += line;
    passed = true;
    for (std::size_t i = 0; i < areas_.size(); ++i)
    {
        const AreaRun &a = areas_[i];
        const umbra::core::FrameStats frame = umbra::core::Summarize(a.frameMs);
        const umbra::core::FrameStats work = umbra::core::Summarize(a.workMs);
        const bool ok = frame.frames > 0 && frame.average <= budgetAverageMs && frame.p99 <= budgetP99Ms;
        passed = passed && ok;
        std::snprintf(line, sizeof(line),
                      "    {\"scene\": \"%s\", \"frames\": %zu, \"fps\": %.1f, \"averageMs\": %.2f, \"p50Ms\": %.2f, \"p95Ms\": %.2f, "
                      "\"p99Ms\": %.2f, \"maxMs\": %.2f, \"cpuWorkAverageMs\": %.2f, \"entities\": %zu, \"withinBudget\": %s}%s\n",
                      a.scene.c_str(), frame.frames, frame.average > 0.0 ? 1000.0 / frame.average : 0.0, frame.average, frame.p50, frame.p95,
                      frame.p99, frame.max, work.average, a.entities, ok ? "true" : "false", i + 1 < areas_.size() ? "," : "");
        out += line;
    }
    out += std::string("  ],\n  \"passed\": ") + (passed ? "true" : "false") + "\n}\n";
    return out;
}
