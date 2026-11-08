#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Lighting.hpp>

#include <glm/geometric.hpp>

using namespace umbra::core;
using Catch::Approx;

TEST_CASE("The sun climbs by day and the moon lights the night (T84)", "[lighting]")
{
    EnvironmentSettings settings;
    const Environment noon = EvaluateEnvironment(settings, 12.0F);
    const Environment night = EvaluateEnvironment(settings, 0.0F);
    const Environment dusk = EvaluateEnvironment(settings, 17.6F);
    CHECK(noon.sunDirection.y > 0.85F);           // 65 degrees up
    CHECK(glm::length(noon.sunDirection) == Approx(1.0F));
    CHECK(night.sunDirection.y > 0.85F);          // the moon, opposite the sun
    CHECK(noon.sunColor.r > 10.0F * night.sunColor.r); // moonlight is faint
    CHECK(night.sunColor.b > night.sunColor.r);   // and blue
    CHECK(dusk.sunColor.r > 2.0F * dusk.sunColor.b); // sunset is orange
    CHECK(noon.skyColor.b > noon.skyColor.r);     // a blue sky
    // Just after sunset the light comes from just above the horizon, never from below.
    CHECK(EvaluateEnvironment(settings, 18.05F).sunDirection.y > 0.04F);
    // The hour wraps: 24 and 48 are midnight.
    CHECK(EvaluateEnvironment(settings, 48.0F).sunColor == night.sunColor);
}

TEST_CASE("Indoors there is no sun or sky; fog and tint follow the settings (T84)", "[lighting]")
{
    EnvironmentSettings inside;
    inside.indoor = true;
    inside.fog = 1.0F;
    const Environment e = EvaluateEnvironment(inside, 12.0F);
    CHECK(e.sunColor == glm::vec3{0.0F});
    CHECK_FALSE(e.shadows);
    CHECK(e.fogDensity == Approx(0.04F));
    CHECK(e.skyColor.r < 0.1F);

    EnvironmentSettings clear;
    clear.fog = 0.0F;
    clear.tint = {1.0F, 0.0F, 0.0F};
    const Environment red = EvaluateEnvironment(clear, 12.0F);
    CHECK(red.fogDensity == 0.0F);
    CHECK(red.sunColor.g == 0.0F);
    CHECK(red.sunColor.r > 1.0F);
    CHECK(red.shadows);
}

TEST_CASE("Time moves by the day length and wraps at midnight (T84)", "[lighting]")
{
    CHECK(AdvanceHour(10.0F, 100.0F, 0.0F) == 10.0F);        // the hour stays
    CHECK(AdvanceHour(10.0F, 30.0F, 720.0F) == Approx(11.0F)); // 12 minutes per day: 30 s = 1 h
    CHECK(AdvanceHour(23.5F, 60.0F, 720.0F) == Approx(1.5F));
}

TEST_CASE("The nearest lights are packed, with flicker and the shadow texel size (T84)", "[lighting]")
{
    std::vector<PointLight> lights;
    for (int i = 0; i < 20; ++i)
        lights.push_back({{static_cast<float>(i) * 10.0F, 0.0F, 0.0F}, {1.0F, 0.5F, 0.25F}, 2.0F, 5.0F, 0.0F});
    lights.push_back({{0.0F, 0.0F, 500.0F}, {1.0F, 1.0F, 1.0F}, 1.0F, 1000.0F, 0.0F}); // far, but its range reaches here
    const LightingBlock b = PackLighting(Environment{}, lights, glm::vec3{0.0F}, 1.0F, 2048);
    CHECK(b.counts.x == static_cast<float>(maxLights));
    CHECK(b.counts.y == Approx(1.0F / 2048.0F));
    CHECK(b.lightPositions[0] == glm::vec4{0.0F, 0.0F, 500.0F, 1000.0F}); // nearest by "distance minus range"
    CHECK(b.lightPositions[1] == glm::vec4{0.0F, 0.0F, 0.0F, 5.0F});
    CHECK(glm::vec3(b.lightColors[1]) == glm::vec3{2.0F, 1.0F, 0.5F}); // color * intensity, no flicker
    for (std::size_t n = 1; n < maxLights; ++n)
        CHECK(b.lightPositions[n].x == static_cast<float>(n - 1) * 10.0F); // the 4 farthest are left out

    // Flicker dims a light by up to half, never brightens it.
    for (float t = 0.0F; t < 5.0F; t += 0.37F)
    {
        const float f = FlickerFactor(1.0F, t, 3);
        CHECK(f >= 0.5F);
        CHECK(f <= 1.0F);
    }
    CHECK(FlickerFactor(0.0F, 2.0F, 1) == 1.0F);
}

TEST_CASE("The sun's shadow box centres on the area and steps by whole texels (T84)", "[lighting]")
{
    const glm::vec3 sun = glm::normalize(glm::vec3{0.3F, 0.8F, 0.4F});
    const glm::mat4 m = SunShadowMatrix(sun, {10.0F, 0.0F, -5.0F}, 25.0F, 2048);
    const glm::vec4 c = m * glm::vec4{10.0F, 0.0F, -5.0F, 1.0F};
    CHECK(std::abs(c.x) < 2.0F / 2048.0F * 2.0F); // within a texel of the middle
    CHECK(std::abs(c.y) < 2.0F / 2048.0F * 2.0F);
    CHECK(c.z == Approx(0.5F).margin(0.01F));     // halfway in depth
    // Nearer the sun = smaller depth (drawn on top in the map).
    const glm::vec4 up = m * glm::vec4{glm::vec3{10.0F, 0.0F, -5.0F} + sun * 10.0F, 1.0F};
    CHECK(up.z < c.z);
    // Moving less than a texel leaves the box where it was (no shimmering).
    const glm::mat4 same = SunShadowMatrix(sun, {10.0F + 0.001F, 0.0F, -5.0F}, 25.0F, 2048);
    const glm::vec4 a = m * glm::vec4{3.0F, 1.0F, 2.0F, 1.0F}, b = same * glm::vec4{3.0F, 1.0F, 2.0F, 1.0F};
    CHECK(glm::length(glm::vec2(a) - glm::vec2(b)) < 1e-4F);
}
