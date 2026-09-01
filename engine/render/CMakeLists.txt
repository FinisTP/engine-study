find_package(SDL3 CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)

add_library(UmbraRender STATIC)
add_library(Umbra::Render ALIAS UmbraRender)

target_sources(
    UmbraRender
    PRIVATE
        src/Renderer.cpp
        src/PhysicalDeviceSelection.hpp
        src/VulkanInstance.cpp
        src/VulkanInstance.hpp
        src/VulkanPhysicalDevice.cpp
        src/VulkanPhysicalDevice.hpp
        src/VulkanSurface.cpp
        src/VulkanSurface.hpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Renderer.hpp
)

target_compile_features(UmbraRender PUBLIC cxx_std_20)

target_link_libraries(
    UmbraRender
    PRIVATE
        Umbra::Diagnostics
        Umbra::Platform
        SDL3::SDL3
        volk::volk
        Umbra::Warnings
        Umbra::Sanitizers
)
