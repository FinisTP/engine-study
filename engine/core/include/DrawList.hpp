#pragma once

#include <Handles.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

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

    // Read-only presentation data the world hands to the renderer each frame.
    struct DrawList final
    {
        std::vector<DrawItem> items;
        std::vector<DebugLine> lines;

        void Clear()
        {
            items.clear();
            lines.clear();
        }
    };
}
