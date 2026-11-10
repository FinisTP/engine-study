find_package(glm CONFIG REQUIRED)

# Browser: the same Renderer interface drawn with WebGPU from JavaScript (web/WebRenderer.md).
if(EMSCRIPTEN)
    add_library(UmbraRender STATIC web/RendererWeb.cpp)
    add_library(Umbra::Render ALIAS UmbraRender)
    target_sources(UmbraRender PUBLIC FILE_SET HEADERS BASE_DIRS include FILES include/Renderer.hpp)
    target_compile_features(UmbraRender PUBLIC cxx_std_20)
    target_link_libraries(UmbraRender PUBLIC Umbra::Core PRIVATE Umbra::Diagnostics glm::glm Umbra::Warnings)
    target_link_options(UmbraRender INTERFACE "--js-library=${CMAKE_CURRENT_SOURCE_DIR}/web/umbra_render.js")
    # Custom surface shaders as WGSL (HLSL -> SPIR-V with DXC -> WGSL with naga), embedded at /shaders/surface.
    find_program(UMBRA_DXC_EXECUTABLE NAMES dxc HINTS "$ENV{VULKAN_SDK}/Bin")
    find_program(UMBRA_NAGA_EXECUTABLE NAMES naga HINTS "$ENV{HOME}/.cargo/bin")
    include(SurfaceShaders.cmake)
    umbra_surface_shaders(web surfaceOutputs)
    if(surfaceOutputs)
        add_custom_target(UmbraSurfaceShaders DEPENDS ${surfaceOutputs})
        add_dependencies(UmbraRender UmbraSurfaceShaders)
        target_link_options(UmbraRender INTERFACE "--embed-file=${CMAKE_CURRENT_BINARY_DIR}/surface@/shaders/surface")
    endif()
    return()
endif()

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

find_path(
    UMBRA_VULKAN_ENUM_HELPER_INCLUDE_DIR
    NAMES vulkan/vk_enum_string_helper.h
    HINTS "$ENV{VULKAN_SDK}/Include"
    REQUIRED
)

# The shaders are compiled on the PC (DXC from the Vulkan SDK). The Android build has no DXC: its
# shaders come in game.pack, made from the PC build's shaders folder (apps/umbra/Mobile.md).
# Target Vulkan 1.1 (SPIR-V 1.3) so phones and the Android emulator can load them.
set(UMBRA_SHADER_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")
if(NOT ANDROID)
find_program(
    UMBRA_DXC_EXECUTABLE
    NAMES dxc
    HINTS "$ENV{VULKAN_SDK}/Bin"
    REQUIRED
)

find_program(
    UMBRA_SPIRV_VAL_EXECUTABLE
    NAMES spirv-val
    HINTS "$ENV{VULKAN_SDK}/Bin"
    REQUIRED
)

set(UMBRA_VERTEX_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Scene.vert.spv")
set(UMBRA_FRAGMENT_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Scene.frag.spv")
set(UMBRA_SHADOW_VERTEX_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Shadow.vert.spv")
set(UMBRA_SKY_VERTEX_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Sky.vert.spv")
set(UMBRA_SKY_FRAGMENT_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Sky.frag.spv")
set(UMBRA_OVERLAY_VERTEX_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Overlay.vert.spv")
set(UMBRA_OVERLAY_FRAGMENT_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Overlay.frag.spv")
set(UMBRA_POST_SHADERS
    "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Fullscreen.vert.spv" "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Bright.frag.spv"
    "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Down.frag.spv" "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Up.frag.spv"
    "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Composite.frag.spv"
    "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Skinned.vert.spv" "${UMBRA_SHADER_OUTPUT_DIRECTORY}/ShadowSkinned.vert.spv")

function(umbra_compile_hlsl output source profile entry_point)
    add_custom_command(
        OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${UMBRA_SHADER_OUTPUT_DIRECTORY}"
        COMMAND "${UMBRA_DXC_EXECUTABLE}"
                -spirv
                -fspv-target-env=vulkan1.1
                -fspv-entrypoint-name=main
                -T "${profile}"
                -E "${entry_point}"
                -Fo "${output}"
                "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
        COMMAND "${UMBRA_SPIRV_VAL_EXECUTABLE}" --target-env vulkan1.1 "${output}"
        DEPENDS "${source}"
        COMMENT "Compiling HLSL ${source}:${entry_point}"
        VERBATIM
    )
endfunction()

umbra_compile_hlsl("${UMBRA_VERTEX_SHADER}" shaders/Scene.hlsl vs_6_0 VSMain)
umbra_compile_hlsl("${UMBRA_FRAGMENT_SHADER}" shaders/Scene.hlsl ps_6_0 PSMain)
umbra_compile_hlsl("${UMBRA_SHADOW_VERTEX_SHADER}" shaders/Scene.hlsl vs_6_0 ShadowVS)
umbra_compile_hlsl("${UMBRA_SHADER_OUTPUT_DIRECTORY}/Skinned.vert.spv" shaders/Scene.hlsl vs_6_0 VSSkinned)
umbra_compile_hlsl("${UMBRA_SHADER_OUTPUT_DIRECTORY}/ShadowSkinned.vert.spv" shaders/Scene.hlsl vs_6_0 ShadowSkinnedVS)
umbra_compile_hlsl("${UMBRA_SKY_VERTEX_SHADER}" shaders/Scene.hlsl vs_6_0 SkyVS)
umbra_compile_hlsl("${UMBRA_SKY_FRAGMENT_SHADER}" shaders/Scene.hlsl ps_6_0 SkyPS)
umbra_compile_hlsl("${UMBRA_OVERLAY_VERTEX_SHADER}" shaders/Overlay.hlsl vs_6_0 VSMain)
umbra_compile_hlsl("${UMBRA_OVERLAY_FRAGMENT_SHADER}" shaders/Overlay.hlsl ps_6_0 PSMain)
umbra_compile_hlsl("${UMBRA_SHADER_OUTPUT_DIRECTORY}/Fullscreen.vert.spv" shaders/Post.hlsl vs_6_0 FullscreenVS)
umbra_compile_hlsl("${UMBRA_SHADER_OUTPUT_DIRECTORY}/Bright.frag.spv" shaders/Post.hlsl ps_6_0 BrightPS)
umbra_compile_hlsl("${UMBRA_SHADER_OUTPUT_DIRECTORY}/Down.frag.spv" shaders/Post.hlsl ps_6_0 DownPS)
umbra_compile_hlsl("${UMBRA_SHADER_OUTPUT_DIRECTORY}/Up.frag.spv" shaders/Post.hlsl ps_6_0 UpPS)
umbra_compile_hlsl("${UMBRA_SHADER_OUTPUT_DIRECTORY}/Composite.frag.spv" shaders/Post.hlsl ps_6_0 CompositePS)

include(SurfaceShaders.cmake)
umbra_surface_shaders(vulkan surfaceOutputs)

add_custom_target(
    UmbraRenderShaders
    DEPENDS "${UMBRA_VERTEX_SHADER}" "${UMBRA_FRAGMENT_SHADER}"
            "${UMBRA_SHADOW_VERTEX_SHADER}" "${UMBRA_SKY_VERTEX_SHADER}" "${UMBRA_SKY_FRAGMENT_SHADER}"
            "${UMBRA_OVERLAY_VERTEX_SHADER}" "${UMBRA_OVERLAY_FRAGMENT_SHADER}" ${UMBRA_POST_SHADERS} ${surfaceOutputs}
)
endif()

add_library(UmbraRender STATIC)
add_library(Umbra::Render ALIAS UmbraRender)

target_sources(
    UmbraRender
    PRIVATE
        src/Renderer.cpp
        src/DiagnosticsOverlay.cpp
        src/DiagnosticsOverlay.hpp
        src/PhysicalDeviceSelection.hpp
        src/RenderResources.cpp
        src/RenderResources.hpp
        src/VulkanDevice.cpp
        src/VulkanPost.cpp
        src/VulkanPost.hpp
        src/VulkanDevice.hpp
        src/VulkanFrameResources.cpp
        src/VulkanFrameResources.hpp
        src/VulkanInstance.cpp
        src/VulkanInstance.hpp
        src/VulkanPhysicalDevice.cpp
        src/VulkanPhysicalDevice.hpp
        src/VulkanSurface.cpp
        src/VulkanSurface.hpp
        src/VulkanSwapchain.cpp
        src/VulkanSwapchain.hpp
        src/VulkanVertexPipeline.cpp
        src/VulkanVertexPipeline.hpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Renderer.hpp
)

target_compile_features(UmbraRender PUBLIC cxx_std_20)

if(TARGET UmbraRenderShaders)
    add_dependencies(UmbraRender UmbraRenderShaders)
endif()

target_compile_definitions(
    UmbraRender
    PRIVATE
        UMBRA_SHADER_DIRECTORY="${UMBRA_SHADER_OUTPUT_DIRECTORY}"
)

target_include_directories(
    UmbraRender
    PRIVATE
        ${UMBRA_VULKAN_ENUM_HELPER_INCLUDE_DIR}
)

target_link_libraries(
    UmbraRender
    PUBLIC
        Umbra::Core
    PRIVATE
        Umbra::Diagnostics
        Umbra::Platform
        glm::glm
        imgui::imgui
        SDL3::SDL3
        volk::volk
        GPUOpen::VulkanMemoryAllocator
        Umbra::Warnings
        Umbra::Sanitizers
)

# Package layout: compiled shaders in shaders/ next to the program (Renderer looks there first).
if(NOT ANDROID)
install(DIRECTORY "${UMBRA_SHADER_OUTPUT_DIRECTORY}/" DESTINATION shaders COMPONENT game FILES_MATCHING PATTERN "*.spv")
endif()
