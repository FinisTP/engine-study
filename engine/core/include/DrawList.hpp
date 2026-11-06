#pragma once

#include <Handles.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

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
        std::string prompt;                // bottom center, e.g. "[E] Read sign"
        std::string message;               // above the prompt, after using something
        std::vector<std::string> warnings; // top right, e.g. missing assets
        std::string menuTitle;             // a centered menu is shown when menuItems is not empty
        std::vector<std::string> menuItems;
        int menuSelected = 0;
        std::vector<std::string> stats;    // extra lines for the diagnostics window (timings, counters)
    };

    // Read-only presentation data the world hands to the renderer each frame.
    struct DrawList final
    {
        Camera camera;
        std::vector<DrawItem> items;
        std::vector<DebugLine> lines;
        Hud hud;

        void Clear()
        {
            items.clear();
            lines.clear();
            hud = {};
        }
    };
}
