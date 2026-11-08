find_package(glm CONFIG REQUIRED)

# Browser: the same Renderer interface drawn with WebGPU from JavaScript (web/WebRenderer.md).
if(EMSCRIPTEN)
    add_library(UmbraRender STATIC web/RendererWeb.cpp)
    add_library(Umbra::Render ALIAS UmbraRender)
    target_sources(UmbraRender PUBLIC FILE_SET HEADERS BASE_DIRS include FILES include/Renderer.hpp)
    target_compile_features(UmbraRender PUBLIC cxx_std_20)
    target_link_libraries(UmbraRender PUBLIC Umbra::Core PRIVATE Umbra::Diagnostics glm::glm Umbra::Warnings)
    target_link_options(UmbraRender INTERFACE "--js-library=${CMAKE_CURRENT_SOURCE_DIR}/web/umbra_render.js")
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

set(UMBRA_SHADER_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(UMBRA_VERTEX_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Scene.vert.spv")
set(UMBRA_FRAGMENT_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Scene.frag.spv")
set(UMBRA_OVERLAY_VERTEX_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Overlay.vert.spv")
set(UMBRA_OVERLAY_FRAGMENT_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Overlay.frag.spv")

function(umbra_compile_hlsl output source profile entry_point)
    add_custom_command(
        OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${UMBRA_SHADER_OUTPUT_DIRECTORY}"
        COMMAND "${UMBRA_DXC_EXECUTABLE}"
                -spirv
                -fspv-target-env=vulkan1.3
                -fspv-entrypoint-name=main
                -T "${profile}"
                -E "${entry_point}"
                -Fo "${output}"
                "${CMAKE_CURRENT_SOURCE_DIR}/${source}"
        COMMAND "${UMBRA_SPIRV_VAL_EXECUTABLE}" --target-env vulkan1.3 "${output}"
        DEPENDS "${source}"
        COMMENT "Compiling HLSL ${source}:${entry_point}"
        VERBATIM
    )
endfunction()

umbra_compile_hlsl("${UMBRA_VERTEX_SHADER}" shaders/Scene.hlsl vs_6_0 VSMain)
umbra_compile_hlsl("${UMBRA_FRAGMENT_SHADER}" shaders/Scene.hlsl ps_6_0 PSMain)
umbra_compile_hlsl("${UMBRA_OVERLAY_VERTEX_SHADER}" shaders/Overlay.hlsl vs_6_0 VSMain)
umbra_compile_hlsl("${UMBRA_OVERLAY_FRAGMENT_SHADER}" shaders/Overlay.hlsl ps_6_0 PSMain)

add_custom_target(
    UmbraRenderShaders
    DEPENDS "${UMBRA_VERTEX_SHADER}" "${UMBRA_FRAGMENT_SHADER}"
            "${UMBRA_OVERLAY_VERTEX_SHADER}" "${UMBRA_OVERLAY_FRAGMENT_SHADER}"
)

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

add_dependencies(UmbraRender UmbraRenderShaders)

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
install(
    FILES "${UMBRA_VERTEX_SHADER}" "${UMBRA_FRAGMENT_SHADER}" "${UMBRA_OVERLAY_VERTEX_SHADER}" "${UMBRA_OVERLAY_FRAGMENT_SHADER}"
    DESTINATION shaders
    COMPONENT game
)
