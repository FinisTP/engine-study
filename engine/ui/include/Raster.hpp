#pragma once

#include <DrawList.hpp>
#include <MeshData.hpp>

#include <functional>

namespace umbra::ui
{
    // Draws UI triangles into a picture on the CPU, the same way the GPU does (texture times
    // vertex color, alpha blending, clip rectangles). For previews without a window: UmbraTool
    // ui-preview, tests. Slow but simple; see UI.md "Raster".
    using TextureLookup = std::function<const core::TextureData *(core::TextureHandle)>;
    [[nodiscard]] core::TextureData Rasterize(const core::UiDrawList &list, int width, int height, const TextureLookup &textures,
                                              glm::vec4 background = {0.05F, 0.06F, 0.09F, 1.0F});
}
