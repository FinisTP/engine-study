#include <umbra/core/FixedStepAccumulator.hpp>
#include <algorithm>
#include <utility>

namespace umbra::core
{

    // add time from the frame clock
    void FixedStepAccumulator::AddDeltaTime(double deltaTime) noexcept {
        if (deltaTime <= 0.0) {
            return;
        }
        accumulator_ += deltaTime;
    }

    // returns true if the accumulator has enough time for a fixed step, 
    // and subtracts one fixed step from the accumulator
    bool FixedStepAccumulator::MarkFixedStep() noexcept {
        bool hasFixedStep = false;
        if (accumulator_ >= fixedStep_) {
            accumulator_ -= fixedStep_;
            interpolationFactor_ = 0.0;
            hasFixedStep = true;
        } else {
            interpolationFactor_ = accumulator_ / fixedStep_;
            interpolationFactor_ = std::clamp(interpolationFactor_, 0.0, 1.0);
        }
        return hasFixedStep;
    }

    // suppose fixed interval = 10ms, accumulator = 15ms, then the interpolation factor = 0.5
    double FixedStepAccumulator::GetInterpolationFactor() const noexcept {
        return interpolationFactor_;
    }
    
    void FixedStepAccumulator::Reset() noexcept {
        accumulator_ = 0.0;
        interpolationFactor_ = 0.0;
    }
}