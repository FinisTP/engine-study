find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h" REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

find_path(
    UMBRA_VULKAN_ENUM_HELPER_INCLUDE_DIR
    NAMES vulkan/vk_enum_string_helper.h
    HINTS "$ENV{VULKAN_SDK}/Include"
    REQUIRED
)

find_program(
    UMBRA_GLSLC_EXECUTABLE
    NAMES glslc
    HINTS "$ENV{VULKAN_SDK}/Bin"
    REQUIRED
)

set(UMBRA_SHADER_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(UMBRA_VERTEX_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Scene.vert.spv")
set(UMBRA_FRAGMENT_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Scene.frag.spv")
set(UMBRA_OVERLAY_VERTEX_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Overlay.vert.spv")
set(UMBRA_OVERLAY_FRAGMENT_SHADER "${UMBRA_SHADER_OUTPUT_DIRECTORY}/Overlay.frag.spv")

add_custom_command(
    OUTPUT "${UMBRA_VERTEX_SHADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${UMBRA_SHADER_OUTPUT_DIRECTORY}"
    COMMAND "${UMBRA_GLSLC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Scene.vert" -o "${UMBRA_VERTEX_SHADER}"
    DEPENDS shaders/Scene.vert
    VERBATIM
)

add_custom_command(
    OUTPUT "${UMBRA_FRAGMENT_SHADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${UMBRA_SHADER_OUTPUT_DIRECTORY}"
    COMMAND "${UMBRA_GLSLC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Scene.frag" -o "${UMBRA_FRAGMENT_SHADER}"
    DEPENDS shaders/Scene.frag
    VERBATIM
)

add_custom_command(
    OUTPUT "${UMBRA_OVERLAY_VERTEX_SHADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${UMBRA_SHADER_OUTPUT_DIRECTORY}"
    COMMAND "${UMBRA_GLSLC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Overlay.vert" -o "${UMBRA_OVERLAY_VERTEX_SHADER}"
    DEPENDS shaders/Overlay.vert
    VERBATIM
)

add_custom_command(
    OUTPUT "${UMBRA_OVERLAY_FRAGMENT_SHADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${UMBRA_SHADER_OUTPUT_DIRECTORY}"
    COMMAND "${UMBRA_GLSLC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Overlay.frag" -o "${UMBRA_OVERLAY_FRAGMENT_SHADER}"
    DEPENDS shaders/Overlay.frag
    VERBATIM
)

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
        src/VulkanTrianglePipeline.cpp
        src/VulkanTrianglePipeline.hpp
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
        UMBRA_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/assets"
)

target_include_directories(
    UmbraRender
    PRIVATE
        ${TINYGLTF_INCLUDE_DIRS}
        ${UMBRA_VULKAN_ENUM_HELPER_INCLUDE_DIR}
)

target_link_libraries(
    UmbraRender
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
