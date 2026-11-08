#pragma once

namespace umbra::gameplay
{
    // How the body is offset this frame: drawn `bob` meters higher and tilted `lean` radians forward.
    struct BodyPose final
    {
        float bob = 0.0F;
        float lean = 0.0F;
    };

    struct StrideSettings final
    {
        float stepLength = 0.9F;     // meters per footstep
        float bobHeight = 0.05F;     // meters at full speed
        float fullBobSpeed = 4.0F;   // m/s where the bob reaches bobHeight
        float maxLean = 0.12F;       // radians at fullLeanSpeed
        float fullLeanSpeed = 6.5F;  // m/s
        float smoothing = 10.0F;     // higher = pose follows speed changes faster
    };

    // Procedural walk animation (no skeleton): the body bobs once per step and leans
    // into its speed. The math is in StrideAnimator.md.
    class StrideAnimator final
    {
    public:
        explicit StrideAnimator(StrideSettings settings = {}) : settings_(settings) {}

        // speed = horizontal speed in m/s. Returns true when a foot lands (play a footstep).
        bool Update(float speed, bool grounded, float deltaSeconds);
        [[nodiscard]] BodyPose GetPose() const;
        void Reset() noexcept { phase_ = amount_ = lean_ = 0.0F; }

    private:
        StrideSettings settings_;
        float phase_ = 0.0F;  // radians; one footstep per pi
        float amount_ = 0.0F; // 0..1, how much bob is shown
        float lean_ = 0.0F;   // radians
    };
}
