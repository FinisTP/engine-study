#include "Lighting.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace umbra::core
{
    namespace
    {
        constexpr float pi = 3.14159265358979F;

        // The look of one hour of the day (Lighting.md): sun or moon light, sky straight up, sky at
        // the horizon, light bounced from the ground, how much the sky lights the scene.
        struct Key final
        {
            float hour;
            glm::vec3 light, zenith, horizon, ground;
            float ambient;
        };
        const Key keys[] = {
            {0.0F, {0.05F, 0.065F, 0.11F}, {0.012F, 0.018F, 0.040F}, {0.030F, 0.035F, 0.065F}, {0.012F, 0.012F, 0.018F}, 1.0F},
            {5.4F, {0.04F, 0.05F, 0.09F}, {0.025F, 0.030F, 0.070F}, {0.080F, 0.070F, 0.120F}, {0.020F, 0.020F, 0.030F}, 1.0F},
            {6.0F, {0.45F, 0.22F, 0.12F}, {0.100F, 0.120F, 0.250F}, {0.600F, 0.350F, 0.300F}, {0.080F, 0.070F, 0.070F}, 0.8F},
            {6.8F, {0.95F, 0.60F, 0.35F}, {0.220F, 0.300F, 0.550F}, {0.850F, 0.600F, 0.450F}, {0.180F, 0.150F, 0.130F}, 0.85F},
            {9.0F, {1.15F, 1.05F, 0.90F}, {0.250F, 0.450F, 0.850F}, {0.650F, 0.750F, 0.900F}, {0.250F, 0.220F, 0.180F}, 1.0F},
            {12.0F, {1.25F, 1.20F, 1.10F}, {0.220F, 0.450F, 0.900F}, {0.620F, 0.740F, 0.920F}, {0.260F, 0.230F, 0.190F}, 1.0F},
            {16.0F, {1.15F, 1.00F, 0.80F}, {0.250F, 0.420F, 0.800F}, {0.700F, 0.720F, 0.820F}, {0.250F, 0.210F, 0.170F}, 1.0F},
            {17.4F, {0.95F, 0.55F, 0.28F}, {0.200F, 0.250F, 0.500F}, {0.900F, 0.550F, 0.350F}, {0.180F, 0.130F, 0.100F}, 0.85F},
            {18.0F, {0.45F, 0.20F, 0.10F}, {0.100F, 0.100F, 0.250F}, {0.600F, 0.300F, 0.250F}, {0.080F, 0.060F, 0.060F}, 0.8F},
            {18.6F, {0.04F, 0.05F, 0.09F}, {0.035F, 0.035F, 0.090F}, {0.110F, 0.075F, 0.130F}, {0.025F, 0.020F, 0.025F}, 1.0F},
            {20.0F, {0.05F, 0.065F, 0.11F}, {0.012F, 0.018F, 0.040F}, {0.030F, 0.035F, 0.065F}, {0.012F, 0.012F, 0.018F}, 1.0F},
            {24.0F, {0.05F, 0.065F, 0.11F}, {0.012F, 0.018F, 0.040F}, {0.030F, 0.035F, 0.065F}, {0.012F, 0.012F, 0.018F}, 1.0F},
        };

        float Wrap24(float hour)
        {
            const float h = std::fmod(hour, 24.0F);
            return h < 0.0F ? h + 24.0F : h;
        }

        // The two keys around `hour` and how far between them (0..1).
        Key Blend(float hour)
        {
            std::size_t i = 0;
            while (keys[i + 1].hour < hour)
                ++i;
            const Key &a = keys[i], &b = keys[i + 1];
            const float t = (hour - a.hour) / (b.hour - a.hour);
            return {hour, glm::mix(a.light, b.light, t), glm::mix(a.zenith, b.zenith, t), glm::mix(a.horizon, b.horizon, t), glm::mix(a.ground, b.ground, t),
                    a.ambient + (b.ambient - a.ambient) * t};
        }
    }

    Environment EvaluateEnvironment(const EnvironmentSettings &settings, float hour)
    {
        Environment e;
        const float fog = std::clamp(settings.fog, 0.0F, 1.0F);
        e.fogDensity = 0.04F * fog * fog;
        e.shadows = settings.shadows && !settings.indoor;
        if (settings.indoor)
        {
            // 1. Inside: no sun and no sky, a faint light everywhere so nothing is pure black.
            e.sunColor = glm::vec3{0.0F};
            e.skyColor = e.horizonColor = settings.tint * 0.06F;
            e.groundColor = settings.tint * 0.035F;
            e.fogColor = glm::vec3{0.01F, 0.01F, 0.015F};
            e.ambient = settings.ambient;
            return e;
        }
        // 2. Where the sun is: up at noon, under the horizon at night (then the moon, opposite, lights the scene).
        const float h = Wrap24(hour);
        const float elevation = std::sin((h - 6.0F) / 12.0F * pi) * glm::radians(65.0F);
        const float azimuth = glm::radians(settings.sunHeading) + (h - 12.0F) / 12.0F * pi;
        const glm::vec3 sun{std::cos(elevation) * std::sin(azimuth), std::sin(elevation), std::cos(elevation) * std::cos(azimuth)};
        e.sunDirection = elevation >= 0.0F ? sun : -sun;
        if (e.sunDirection.y < 0.05F) // at sunrise and sunset: light only just above the horizon
            e.sunDirection = glm::normalize(glm::vec3{e.sunDirection.x, 0.05F, e.sunDirection.z});
        // 3. Its colors at this hour, tinted.
        const Key key = Blend(h);
        e.sunColor = key.light * settings.tint;
        e.skyColor = key.zenith * settings.tint;
        e.horizonColor = key.horizon * settings.tint;
        e.groundColor = key.ground * settings.tint;
        e.ambient = key.ambient * settings.ambient;
        e.fogColor = e.horizonColor;
        return e;
    }

    float AdvanceHour(float hour, float seconds, float dayLength)
    {
        if (dayLength <= 0.0F)
            return hour;
        return Wrap24(hour + seconds * 24.0F / dayLength);
    }

    float FlickerFactor(float flicker, float seconds, std::size_t index)
    {
        // Two sines at unrelated speeds, shifted per light, look like a flame (0..0.5 dimming).
        const float i = static_cast<float>(index);
        const float wave = 0.25F * (std::sin(seconds * 11.0F + i * 1.7F) + 1.0F) * 0.5F + 0.25F * (std::sin(seconds * 23.3F + i * 4.1F) + 1.0F) * 0.5F;
        return 1.0F - std::clamp(flicker, 0.0F, 1.0F) * wave;
    }

    LightingBlock PackLighting(const Environment &environment, const std::vector<PointLight> &lights, glm::vec3 camera, float seconds,
                               std::size_t shadowMapSize)
    {
        LightingBlock b;
        b.cameraPosition = {camera, seconds};
        b.sunDirection = {glm::normalize(environment.sunDirection), environment.shadows ? 1.0F : 0.0F};
        b.sunColor = {environment.sunColor, 0.0F};
        b.skyColor = {environment.skyColor, environment.ambient};
        b.horizonColor = {environment.horizonColor, 0.0F};
        b.groundColor = {environment.groundColor, 0.0F};
        b.fogColor = {environment.fogColor, environment.fogDensity};

        // 1. The lights that matter most here: nearest first (a big light counts as nearer).
        std::vector<std::size_t> order(lights.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        const auto score = [&](std::size_t i) { return glm::distance(lights[i].position, camera) - lights[i].range; };
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t c) { return score(a) < score(c); });
        const std::size_t used = std::min(order.size(), maxLights);
        // 2. Each one: position and range, color times intensity times flicker.
        for (std::size_t n = 0; n < used; ++n)
        {
            const PointLight &light = lights[order[n]];
            b.lightPositions[n] = {light.position, std::max(light.range, 0.01F)};
            b.lightColors[n] = {light.color * light.intensity * FlickerFactor(light.flicker, seconds, order[n]), 0.0F};
        }
        b.counts = {static_cast<float>(used), shadowMapSize > 0 ? 1.0F / static_cast<float>(shadowMapSize) : 0.0F, 0.0F, 0.0F};
        return b;
    }

    glm::mat4 SunShadowMatrix(glm::vec3 sunDirection, glm::vec3 center, float radius, std::size_t mapSize)
    {
        // 1. Look from the sun towards the ground, from the origin (so snapping works in light space).
        const glm::vec3 dir = glm::normalize(sunDirection);
        const glm::vec3 up = std::abs(dir.y) > 0.99F ? glm::vec3{0.0F, 0.0F, 1.0F} : glm::vec3{0.0F, 1.0F, 0.0F};
        const glm::mat4 view = glm::lookAtRH(glm::vec3{0.0F}, -dir, up);
        // 2. The area's center in light space, moved to a whole texel.
        glm::vec3 c = glm::vec3(view * glm::vec4(center, 1.0F));
        const float texel = 2.0F * radius / static_cast<float>(std::max<std::size_t>(mapSize, 1));
        c.x = std::floor(c.x / texel) * texel;
        c.y = std::floor(c.y / texel) * texel;
        // 3. A box around it; depth reaches 3 radii towards and away from the sun (tall things cast too).
        const float distance = -c.z;
        return glm::orthoRH_ZO(c.x - radius, c.x + radius, c.y - radius, c.y + radius, distance - 3.0F * radius, distance + 3.0F * radius) * view;
    }
}
