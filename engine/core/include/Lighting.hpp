#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstddef>
#include <vector>

// Lights and the air around them (roadmap S6). The scene describes them (the "light" and
// "environment" components); these functions turn that into what both renderers upload, so the
// Vulkan and WebGPU pictures use the same numbers. The math: Lighting.md.
namespace umbra::core
{
    // A point light (torch, fire, crystal): light falls off to nothing at `range` meters.
    struct PointLight final
    {
        glm::vec3 position{0.0F};
        glm::vec3 color{1.0F, 0.75F, 0.45F};
        float intensity = 3.0F;
        float range = 8.0F;
        float flicker = 0.0F; // 0 = steady, 1 = strong flicker (fire)
    };

    // The scene's "environment" component: time of day, inside or outside, fog.
    struct EnvironmentSettings final
    {
        float hour = 14.0F;      // 0..24; 6 = sunrise, 12 = noon, 18 = sunset
        float dayLength = 0.0F;  // seconds of play for a whole day; 0 = the hour stays
        float sunHeading = 30.0F; // degrees: where the sun is at noon, around the up axis
        bool indoor = false;     // no sun or sky: only a dim ambient light and the point lights
        float ambient = 1.0F;    // multiplies the sky's light
        float fog = 0.3F;        // 0 = clear air, 1 = thick fog
        glm::vec3 tint{1.0F};    // multiplies every light (warm, cold, eerie...)
        bool shadows = true;     // the sun (or moon) casts shadows
    };

    // What the renderer uses this frame.
    struct Environment final
    {
        glm::vec3 sunDirection{0.4F, 0.8F, 0.5F}; // unit vector towards the sun (or moon)
        glm::vec3 sunColor{1.0F};                 // color times intensity; 0 = none
        glm::vec3 skyColor{0.3F, 0.45F, 0.8F};    // straight up
        glm::vec3 horizonColor{0.7F, 0.75F, 0.85F};
        glm::vec3 groundColor{0.25F, 0.22F, 0.18F};
        float ambient = 1.0F;
        glm::vec3 fogColor{0.7F, 0.75F, 0.85F};
        float fogDensity = 0.0F; // per meter
        bool shadows = true;
    };

    // The sky, sun and fog at `hour` (Lighting.md, "Day and night").
    [[nodiscard]] Environment EvaluateEnvironment(const EnvironmentSettings &settings, float hour);
    // The hour after `seconds` of play, wrapped to 0..24 (unchanged if dayLength is 0).
    [[nodiscard]] float AdvanceHour(float hour, float seconds, float dayLength);

    // The uniform block both renderers upload (std140: every member is a vec4).
    inline constexpr std::size_t maxLights = 16;
    struct LightingBlock final
    {
        glm::vec4 cameraPosition{0.0F}; // w = time in seconds
        glm::vec4 sunDirection{0.0F};   // w = 1 if the sun casts shadows
        glm::vec4 sunColor{0.0F};
        glm::vec4 skyColor{0.0F};       // w = ambient
        glm::vec4 horizonColor{0.0F};
        glm::vec4 groundColor{0.0F};
        glm::vec4 fogColor{0.0F};       // w = density
        glm::vec4 counts{0.0F};         // x = lights used, y = shadow texel size (1 / map size)
        std::array<glm::vec4, maxLights> lightPositions{}; // xyz, w = range
        std::array<glm::vec4, maxLights> lightColors{};    // color * intensity (flicker applied)
    };

    // Picks the lights nearest the camera (at most maxLights) and fills the block.
    [[nodiscard]] LightingBlock PackLighting(const Environment &environment, const std::vector<PointLight> &lights, glm::vec3 camera,
                                             float seconds, std::size_t shadowMapSize);
    // How much a light's flicker dims it now: 1 = full, lower = dimmer.
    [[nodiscard]] float FlickerFactor(float flicker, float seconds, std::size_t index);

    // The sun's view of the area around `center` (a square `radius` meters wide each way), as one
    // matrix: world -> shadow map clip space (depth 0..1). Snapped to whole texels so shadows
    // don't shimmer when the camera moves.
    [[nodiscard]] glm::mat4 SunShadowMatrix(glm::vec3 sunDirection, glm::vec3 center, float radius, std::size_t mapSize);
}
