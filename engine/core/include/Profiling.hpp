#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>

namespace umbra::core
{
    // Average and worst of the last `window` samples (e.g. milliseconds per frame).
    // A single frame's time jumps around; the average shows the trend, the max shows hitches.
    class RollingTimer final
    {
    public:
        static constexpr std::size_t window = 120; // about 2 seconds at 60 FPS

        void Add(double sample) noexcept
        {
            samples_[next_] = sample;
            next_ = (next_ + 1) % window;
            count_ = std::min(count_ + 1, window);
        }

        [[nodiscard]] double Average() const noexcept
        {
            double sum = 0.0;
            for (std::size_t i = 0; i < count_; ++i)
                sum += samples_[i];
            return count_ ? sum / static_cast<double>(count_) : 0.0;
        }

        [[nodiscard]] double Max() const noexcept
        {
            return count_ ? *std::max_element(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(count_)) : 0.0;
        }

    private:
        std::array<double, window> samples_{};
        std::size_t next_ = 0;  // where the next sample goes (oldest is overwritten)
        std::size_t count_ = 0; // valid samples, up to window
    };

    // Milliseconds since construction or the last Restart().
    class Stopwatch final
    {
    public:
        [[nodiscard]] double Milliseconds() const
        {
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_).count();
        }
        // Returns the elapsed time and starts over (for timing consecutive sections).
        double Lap()
        {
            const double elapsed = Milliseconds();
            start_ = std::chrono::steady_clock::now();
            return elapsed;
        }

    private:
        std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
    };
}
