find_package(Catch2 3 CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)

add_executable(UmbraRenderTests)

target_sources(
    UmbraRenderTests
    PRIVATE
        PhysicalDeviceSelectionTests.cpp
)

target_include_directories(
    UmbraRenderTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/render/src
)

target_compile_features(UmbraRenderTests PRIVATE cxx_std_20)

target_link_libraries(
    UmbraRenderTests
    PRIVATE
        Umbra::Render
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
        volk::volk_headers
)

umbra_copy_asan_runtime(UmbraRenderTests)

add_test(
    NAME Umbra.Render
    COMMAND UmbraRenderTests
)

# T86: a custom surface shader with a mistake is refused at its own line (CustomShaders.md).
if(UMBRA_DXC_EXECUTABLE)
    add_test(
        NAME Umbra.Render.SurfaceShaderErrors
        COMMAND ${CMAKE_COMMAND} -DDXC=${UMBRA_DXC_EXECUTABLE} -DSHADERS=${PROJECT_SOURCE_DIR}/engine/render/shaders
                -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/data/broken.surface.hlsl -DWORK=${CMAKE_CURRENT_BINARY_DIR}/surface-errors
                -P ${CMAKE_CURRENT_SOURCE_DIR}/SurfaceErrors.cmake
    )
endif()
