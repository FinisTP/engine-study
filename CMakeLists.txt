cmake_minimum_required(VERSION 3.28)

project(
    Umbra
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(UmbraWarnings)
include(UmbraSanitizers)
include(UmbraHeadlessCheck)

add_subdirectory(engine/core)
add_subdirectory(engine/diagnostics)
add_subdirectory(engine/platform)
add_subdirectory(engine/render)
add_subdirectory(engine/world)
add_subdirectory(engine/physics)
add_subdirectory(engine/gameplay)
add_subdirectory(engine/assets)
add_subdirectory(engine/scene)
add_subdirectory(apps/umbra)
add_subdirectory(tools/scenecheck)

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests/core)
    add_subdirectory(tests/platform)
    add_subdirectory(tests/diagnostics)
    add_subdirectory(tests/render)
    add_subdirectory(tests/world)
    add_subdirectory(tests/assets)
    add_subdirectory(tests/scene)
    add_subdirectory(tests/physics)
    add_subdirectory(tests/gameplay)

    # Simulation, assets and scene tools must build and run without a window, GPU, editor or model runtime.
    umbra_add_headless_check(
        Umbra.HeadlessDependencies
        TARGETS Umbra::Core Umbra::World Umbra::Assets Umbra::Scene Umbra::Physics Umbra::Gameplay UmbraSceneCheck
        FORBIDDEN "Umbra(Render|Platform)|SDL3|volk|Vulkan|imgui|onnx|python"
    )
endif()
