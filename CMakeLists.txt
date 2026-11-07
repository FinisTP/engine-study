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
add_subdirectory(engine/rpg)
add_subdirectory(engine/turncombat)
add_subdirectory(engine/realtimecombat)
add_subdirectory(engine/encounter)
add_subdirectory(engine/story)
add_subdirectory(engine/game)
add_subdirectory(engine/audio)
add_subdirectory(engine/platform)
add_subdirectory(engine/render)
add_subdirectory(engine/world)
add_subdirectory(engine/physics)
add_subdirectory(engine/gameplay)
add_subdirectory(engine/assets)
add_subdirectory(engine/scene)
add_subdirectory(engine/authoring)
add_subdirectory(apps/umbra)
add_subdirectory(apps/editor)
add_subdirectory(tools/scenecheck)
add_subdirectory(tools/umbratool)

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
    add_subdirectory(tests/audio)
    add_subdirectory(tests/rpg)
    add_subdirectory(tests/turncombat)
    add_subdirectory(tests/realtimecombat)
    add_subdirectory(tests/encounter)
    add_subdirectory(tests/story)
    add_subdirectory(tests/game)
    add_subdirectory(tests/authoring)

    # Simulation, assets and scene tools must build and run without a window, GPU, editor or model runtime.
    umbra_add_headless_check(
        Umbra.HeadlessDependencies
        TARGETS Umbra::Core Umbra::World Umbra::Assets Umbra::Scene Umbra::Physics Umbra::Gameplay Umbra::Audio Umbra::Rpg Umbra::TurnCombat Umbra::RealtimeCombat Umbra::Encounter Umbra::Story Umbra::Game Umbra::Authoring UmbraSceneCheck UmbraTool
        FORBIDDEN "Umbra(Render|Platform)|SDL3|volk|Vulkan|imgui|onnx|python"
    )

    # The game ships without editor code (roadmap P7.1).
    umbra_add_headless_check(
        Umbra.GameExcludesEditor
        TARGETS Umbra
        FORBIDDEN "UmbraEditor|UmbraAuthoring|imguizmo"
    )

    # Each combat mode works without the other (roadmap T39/T40) and without world/physics.
    umbra_add_headless_check(
        Umbra.TurnOnlyDependencies
        TARGETS UmbraTurnCombatTests
        FORBIDDEN "UmbraRealtimeCombat|Umbra(Render|Platform|World|Physics|Gameplay)|SDL3|Jolt|volk|imgui"
    )
    umbra_add_headless_check(
        Umbra.RealtimeOnlyDependencies
        TARGETS UmbraRealtimeCombatTests
        FORBIDDEN "UmbraTurnCombat|Umbra(Render|Platform|World|Physics|Gameplay)|SDL3|Jolt|volk|imgui"
    )
endif()
