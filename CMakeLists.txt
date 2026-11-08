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

# Static engine libraries also go into the training bridge (a shared library), so build them
# position-independent everywhere.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Browser build (roadmap P10): only the game and the modules it uses, single-threaded, with
# exceptions enabled for nlohmann-json. Configure with emcmake; see apps/umbra/web/README.md.
if(EMSCRIPTEN)
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF) # clang-scan-deps doesn't know the Emscripten sysroot; no modules are used
    add_compile_options(-fexceptions)
    add_link_options(-fexceptions)
    foreach(module core diagnostics rpg turncombat realtimecombat encounter story game policy audio platform render world physics gameplay assets scene)
        add_subdirectory(engine/${module})
    endforeach()
    add_subdirectory(apps/umbra)
    # The headless test suites, run by Node (-sNODERAWFS reads the test data from disk): the same
    # rules must give the same results in the browser (P10.2).
    option(UMBRA_WEB_TESTS "Build the headless tests for Node" OFF)
    if(UMBRA_WEB_TESTS)
        include(CTest)
        add_link_options(-sNODERAWFS=1 -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=4194304 -sEXIT_RUNTIME=1)
        foreach(suite core rpg turncombat realtimecombat encounter story game policy world assets scene physics gameplay)
            add_subdirectory(tests/${suite})
        endforeach()
    endif()
    return()
endif()

add_subdirectory(engine/core)
add_subdirectory(engine/diagnostics)
add_subdirectory(engine/rpg)
add_subdirectory(engine/turncombat)
add_subdirectory(engine/realtimecombat)
add_subdirectory(engine/encounter)
add_subdirectory(engine/story)
add_subdirectory(engine/game)
add_subdirectory(engine/policy)
add_subdirectory(engine/audio)
add_subdirectory(engine/platform)
add_subdirectory(engine/render)
add_subdirectory(engine/world)
add_subdirectory(engine/physics)
add_subdirectory(engine/gameplay)
add_subdirectory(engine/assets)
add_subdirectory(engine/scene)
add_subdirectory(engine/authoring)
add_subdirectory(engine/localai)
add_subdirectory(apps/umbra)
add_subdirectory(apps/editor)
add_subdirectory(tools/scenecheck)
add_subdirectory(tools/umbratool)
add_subdirectory(tools/policy)

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
    add_subdirectory(tests/policy)
    add_subdirectory(tests/localai)

    # Simulation, assets and scene tools must build and run without a window, GPU, editor or model runtime.
    umbra_add_headless_check(
        Umbra.HeadlessDependencies
        TARGETS Umbra::Core Umbra::World Umbra::Assets Umbra::Scene Umbra::Physics Umbra::Gameplay Umbra::Audio Umbra::Rpg Umbra::TurnCombat Umbra::RealtimeCombat Umbra::Encounter Umbra::Story Umbra::Game Umbra::Authoring Umbra::Policy Umbra::LocalAi UmbraSceneCheck UmbraTool
        FORBIDDEN "Umbra(Render|Platform)|SDL3|volk|Vulkan|imgui|onnx|python"
    )

    # The game ships without editor code (roadmap P7.1).
    umbra_add_headless_check(
        Umbra.GameExcludesEditor
        TARGETS Umbra
        FORBIDDEN "UmbraEditor|UmbraAuthoring|imguizmo|UmbraPolicyEnv|UmbraLocalAi|httplib"
    )

    # Engine modules, content checks included, never open network connections (roadmap T58):
    # only the opt-in LocalAi module talks to a local Ollama.
    umbra_add_headless_check(
        Umbra.EngineOffline
        TARGETS Umbra::Core Umbra::Rpg Umbra::Story Umbra::Game Umbra::Scene Umbra::Authoring Umbra::Policy UmbraSceneCheck
        FORBIDDEN "httplib|UmbraLocalAi"
    )

    # The learned enemy is turn-only and runs without any ML runtime or Python (roadmap T52/T53).
    umbra_add_headless_check(
        Umbra.PolicyDependencies
        TARGETS Umbra::Policy
        FORBIDDEN "UmbraRealtimeCombat|Umbra(Render|Platform|World|Physics|Gameplay)|SDL3|Jolt|volk|imgui|onnx|python|torch"
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
