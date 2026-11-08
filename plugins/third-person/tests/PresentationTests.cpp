#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <StrideAnimator.hpp>

using Catch::Approx;
using namespace umbra::gameplay;

TEST_CASE("Footsteps follow distance walked, not frame rate")
{
    // 4 m at 4 m/s with 0.9 m steps = 4 footsteps, at 30 and at 144 FPS.
    for (const float fps : {30.0F, 144.0F})
    {
        StrideAnimator animator;
        int steps = 0;
        for (int i = 0; i < static_cast<int>(fps); ++i)
            steps += animator.Update(4.0F, true, 1.0F / fps) ? 1 : 0;
        REQUIRE(steps == 4);
    }
}

TEST_CASE("No footsteps or bob in the air or when standing")
{
    StrideAnimator animator;
    for (int i = 0; i < 60; ++i)
        REQUIRE_FALSE(animator.Update(5.0F, false, 1.0F / 60.0F));
    REQUIRE(animator.GetPose().bob == Approx(0.0F));
    REQUIRE(animator.GetPose().lean > 0.0F); // still leans into the motion

    for (int i = 0; i < 120; ++i)
        REQUIRE_FALSE(animator.Update(0.0F, true, 1.0F / 60.0F));
    REQUIRE(animator.GetPose().lean == Approx(0.0F).margin(1e-3));
}

TEST_CASE("Bob and lean stay within their limits and grow with speed")
{
    StrideSettings settings;
    StrideAnimator slow(settings), fast(settings);
    float maxBob = 0.0F;
    for (int i = 0; i < 120; ++i)
    {
        slow.Update(1.5F, true, 1.0F / 60.0F);
        fast.Update(6.5F, true, 1.0F / 60.0F);
        maxBob = std::max(maxBob, fast.GetPose().bob);
        REQUIRE(fast.GetPose().bob <= settings.bobHeight + 1e-6F);
    }
    REQUIRE(maxBob > settings.bobHeight * 0.9F);
    REQUIRE(fast.GetPose().lean == Approx(settings.maxLean).margin(1e-3));
    REQUIRE(slow.GetPose().lean < fast.GetPose().lean);
}
