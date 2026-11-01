#pragma once

#include <SlotMap.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <vector>

namespace umbra::core
{
    // Render-neutral resource handles. Gameplay code can hold these without
    // seeing any graphics API type; the renderer resolves them.
    struct MeshTag;
    struct TextureTag;
    using MeshHandle = Handle<MeshTag>;
    using TextureHandle = Handle<TextureTag>;

    // One object to draw this frame.
    struct DrawItem final
    {
        MeshHandle mesh;
        TextureHandle texture; // invalid -> renderer uses its fallback texture
        glm::mat4 model{1.0F};
        glm::vec4 color{1.0F};
    };

    // Read-only presentation data the world hands to the renderer each frame.
    struct DrawList final
    {
        std::vector<DrawItem> items;
    };
}
