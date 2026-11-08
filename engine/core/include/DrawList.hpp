#pragma once

#include <Handles.hpp>
#include <Lighting.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace umbra::core
{
    // One object to draw this frame.
    struct DrawItem final
    {
        MeshHandle mesh;
        TextureHandle texture; // invalid -> renderer uses its fallback texture
        glm::mat4 model{1.0F};
        glm::vec4 color{1.0F};
        float metersPerRepeat = 0.0F; // > 0: texture repeats every N meters in world space (WorldUv.md)
        float emissive = 0.0F;        // glows by itself (Lighting.md)
    };

    // A world-space line for debugging (collider outlines, rays). Drawn on top, no depth test.
    struct DebugLine final
    {
        glm::vec3 from{0.0F};
        glm::vec3 to{0.0F};
        glm::vec4 color{1.0F};
    };

    // Where the frame is seen from. Gameplay decides; the renderer only draws.
    struct Camera final
    {
        glm::vec3 position{0.0F, 1.0F, 5.0F};
        glm::vec3 target{0.0F, 1.0F, 0.0F};
        float fovYDegrees = 60.0F;
        float nearPlane = 0.1F;
        float farPlane = 200.0F;
    };

    // Screen text for this frame. Gameplay fills it; the renderer only draws it.
    struct Hud final
    {
        std::vector<std::string> warnings; // top right, e.g. missing assets (game screens are in `ui`)
        bool diagnosticsWindow = true;     // the "Umbra diagnostics" window (the editor hides it)
        std::vector<std::string> stats;    // extra lines for the diagnostics window (timings, counters)
        std::string state;                 // JSON for tests and the web page: open screens, hero
    };

    // Game UI (S3, engine/ui): screen-space triangles in pixels (0,0 = top left), drawn after the
    // scene in batch order. Vertices match Dear ImGui's layout, so one pipeline draws both.
    struct UiVertex final
    {
        glm::vec2 position{0.0F};
        glm::vec2 uv{0.0F};
        std::uint32_t color = 0xFFFFFFFFu; // RGBA8, red in the low byte
    };

    struct UiBatch final
    {
        TextureHandle texture;
        glm::vec4 clip{0.0F, 0.0F, 1e9F, 1e9F}; // x0, y0, x1, y1 in pixels
        std::uint32_t firstVertex = 0;          // indices count from here (fewer than 65536 vertices per batch)
        std::uint32_t firstIndex = 0;
        std::uint32_t indexCount = 0;
    };

    struct UiDrawList final
    {
        std::vector<UiVertex> vertices;
        std::vector<std::uint16_t> indices; // relative to the batch's firstVertex
        std::vector<UiBatch> batches;

        void Clear()
        {
            vertices.clear();
            indices.clear();
            batches.clear();
        }
        // Adds another list on top (drawn after this one); its indices stay relative to their batch.
        void Append(const UiDrawList &other)
        {
            for (UiBatch batch : other.batches)
            {
                batch.firstVertex += static_cast<std::uint32_t>(vertices.size());
                batch.firstIndex += static_cast<std::uint32_t>(indices.size());
                batches.push_back(batch);
            }
            vertices.insert(vertices.end(), other.vertices.begin(), other.vertices.end());
            indices.insert(indices.end(), other.indices.begin(), other.indices.end());
        }
    };

    // Read-only presentation data the world hands to the renderer each frame.
    struct DrawList final
    {
        Camera camera;
        Environment environment;        // sun, sky, fog (Lighting.md)
        std::vector<PointLight> lights; // torches, fires... (the nearest maxLights are used)
        std::vector<DrawItem> items;
        std::vector<DebugLine> lines;
        Hud hud;
        UiDrawList ui; // the game's own UI; drawn under the diagnostics window

        void Clear()
        {
            items.clear();
            lights.clear();
            lines.clear();
            hud = {};
            ui.Clear();
        }
    };
}
