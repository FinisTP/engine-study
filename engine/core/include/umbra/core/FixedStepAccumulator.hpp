#pragma once

namespace umbra::core
{
    class FixedStepAccumulator
    {
    private:
        double accumulator_ = 0.0;
        const double fixedStep_;
        double interpolationFactor_ = 0.0;

    public:
        // normally it will be 1 / 60
        explicit FixedStepAccumulator(double fixedStep) : fixedStep_(fixedStep) {}

        // add time from the frame clock
        void AddDeltaTime(double deltaTime) noexcept;

        // returns true if the accumulator has enough time for a fixed step, 
        // and subtracts one fixed step from the accumulator
        bool MarkFixedStep() noexcept;

        // suppose fixed interval = 10ms, accumulator = 15ms, then the interpolation factor = 0.5
        double GetInterpolationFactor() const noexcept;

        void Reset() noexcept;
    };
}