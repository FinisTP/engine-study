#include <StrideAnimator.hpp>

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace umbra::gameplay
{
    bool StrideAnimator::Update(float speed, bool grounded, float deltaSeconds)
    {
        // 1. Ease bob amount and lean toward their targets (frame-rate independent).
        const float follow = 1.0F - std::exp(-settings_.smoothing * deltaSeconds);
        const float bobTarget = grounded ? std::clamp(speed / settings_.fullBobSpeed, 0.0F, 1.0F) : 0.0F;
        const float leanTarget = std::clamp(speed / settings_.fullLeanSpeed, 0.0F, 1.0F) * settings_.maxLean;
        amount_ += (bobTarget - amount_) * follow;
        lean_ += (leanTarget - lean_) * follow;

        // 2. Advance the stride only on the ground: pi radians per stepLength traveled.
        if (!grounded)
            return false;
        const float before = std::floor(phase_ / glm::pi<float>());
        phase_ += glm::pi<float>() * speed * deltaSeconds / settings_.stepLength;
        phase_ = std::fmod(phase_, 2.0F * glm::pi<float>() * 1000.0F); // keep the float precise

        // 3. A foot lands each time the phase crosses a multiple of pi (the low point of the bob).
        //    Very slow shuffling makes no sound.
        const float after = std::floor(phase_ / glm::pi<float>());
        return after != before && speed > 0.5F;
    }

    BodyPose StrideAnimator::GetPose() const
    {
        return {settings_.bobHeight * amount_ * std::abs(std::sin(phase_)), lean_};
    }
}
