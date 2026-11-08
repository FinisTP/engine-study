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

# Layers (docs/adr/0004-project-layers.md, 0005-plugins.md): engine/ = base engine; plugins/<name>/
# = the game systems and tools, each a self-contained source plugin (RPG rules, combat modes,
# story, learned AI, AI authoring...); projects/<name>/ = one game: project.json (its plugins),
# assets, and its own engine/ and game/ code. The tests use the reference project.
set(UMBRA_PROJECT "reference" CACHE STRING "Project under projects/ that the game and editor use")
set(UMBRA_PROJECT_FOLDER "" CACHE PATH "Optional: a project folder outside projects/ (overrides UMBRA_PROJECT)")
set(UMBRA_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/projects/${UMBRA_PROJECT}")
if(UMBRA_PROJECT_FOLDER)
    set(UMBRA_PROJECT_DIR "${UMBRA_PROJECT_FOLDER}")
endif()
set(UMBRA_ASSETS "${UMBRA_PROJECT_DIR}/assets")
set(UMBRA_REFERENCE_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/projects/reference/assets")
set(UMBRA_ENGINE_MODULES core diagnostics audio platform render world physics assets scene)
include(UmbraPlugins)
umbra_load_plugins()
umbra_plugin_asset_dirs(UMBRA_PLUGIN_ASSETS)
list(JOIN UMBRA_PLUGIN_ASSETS "|" UMBRA_PLUGIN_ASSETS_TEXT) # for the editor and UmbraTool (development programs)
list(JOIN UMBRA_PLUGINS "|" UMBRA_PLUGINS_TEXT)             # the game: names only, folders found at run time
set(UMBRA_PLUGIN_DIRS_TEXT)
foreach(p IN LISTS UMBRA_PLUGINS)
    list(APPEND UMBRA_PLUGIN_DIRS_TEXT "${p}=${UMBRA_PLUGIN_DIR_${p}}")
endforeach()
list(JOIN UMBRA_PLUGIN_DIRS_TEXT "|" UMBRA_PLUGIN_DIRS_TEXT) # the editor's Code window

# Browser build (roadmap P10): only the game and the modules it uses, single-threaded, with
# exceptions enabled for nlohmann-json. Configure with emcmake; see plugins/web-export/web/README.md.
if(EMSCRIPTEN)
    umbra_require_plugins("The browser build" web-export)
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF) # clang-scan-deps doesn't know the Emscripten sysroot; no modules are used
    add_compile_options(-fexceptions)
    add_link_options(-fexceptions)
    foreach(module ${UMBRA_ENGINE_MODULES})
        add_subdirectory(engine/${module})
    endforeach()
    umbra_add_plugin_modules(KIND game)
    add_subdirectory(apps/umbra)
    # The headless test suites, run by Node (-sNODERAWFS reads the test data from disk): the same
    # rules must give the same results in the browser (P10.2).
    option(UMBRA_WEB_TESTS "Build the headless tests for Node" OFF)
    if(UMBRA_WEB_TESTS)
        include(CTest)
        add_link_options(-sNODERAWFS=1 -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=4194304 -sEXIT_RUNTIME=1)
        foreach(suite core world assets scene physics)
            add_subdirectory(tests/${suite})
        endforeach()
        foreach(p IN LISTS UMBRA_PLUGINS)
            if(UMBRA_PLUGIN_KIND_${p} STREQUAL "game" AND EXISTS ${UMBRA_PLUGIN_DIR_${p}}/tests/CMakeLists.txt)
                add_subdirectory(${UMBRA_PLUGIN_DIR_${p}}/tests ${CMAKE_BINARY_DIR}/plugins/${p}/tests)
            endif()
        endforeach()
    endif()
    return()
endif()

foreach(module ${UMBRA_ENGINE_MODULES})
    add_subdirectory(engine/${module})
endforeach()
# Tool library for the editor and UmbraTool (content documents, validation, packaging); never in the game.
add_subdirectory(engine/authoring)
umbra_add_plugin_modules()
umbra_add_plugin_folders(tools)
add_subdirectory(apps/umbra)
add_subdirectory(apps/editor)
add_subdirectory(tools/scenecheck)
add_subdirectory(tools/umbratool)

include(CTest)

if(BUILD_TESTING)
    add_compile_definitions("UMBRA_TEST_PLUGIN_ASSETS=\"${UMBRA_PLUGIN_ASSETS_TEXT}\"") # test::ReferenceAssets()
    foreach(suite core platform diagnostics render world assets scene physics audio authoring app)
        add_subdirectory(tests/${suite})
    endforeach()
    umbra_add_plugin_folders(tests)

    # Dependency gates over the targets this project builds (a turned-off plugin has none).
    function(umbra_existing out)
        set(found)
        foreach(t IN LISTS ARGN)
            if(TARGET ${t})
                list(APPEND found ${t})
            endif()
        endforeach()
        set(${out} ${found} PARENT_SCOPE)
    endfunction()

    # Simulation, assets and scene tools must build and run without a window, GPU, editor or model runtime.
    umbra_existing(headless Umbra::Core Umbra::World Umbra::Assets Umbra::Scene Umbra::Physics Umbra::Gameplay Umbra::Audio Umbra::Rpg Umbra::TurnCombat
                   Umbra::RealtimeCombat Umbra::Encounter Umbra::Story Umbra::Game Umbra::Authoring Umbra::Policy Umbra::QuestDraft Umbra::LocalAi UmbraSceneCheck UmbraTool)
    umbra_add_headless_check(
        Umbra.HeadlessDependencies
        TARGETS ${headless}
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
    umbra_existing(offline Umbra::Core Umbra::Rpg Umbra::Story Umbra::Game Umbra::Scene Umbra::Authoring Umbra::Policy UmbraSceneCheck)
    umbra_add_headless_check(
        Umbra.EngineOffline
        TARGETS ${offline}
        FORBIDDEN "httplib|UmbraLocalAi"
    )

    # The learned enemy is turn-only and runs without any ML runtime or Python (roadmap T52/T53).
    if(TARGET Umbra::Policy)
        umbra_add_headless_check(
            Umbra.PolicyDependencies
            TARGETS Umbra::Policy
            FORBIDDEN "UmbraRealtimeCombat|Umbra(Render|Platform|World|Physics|Gameplay)|SDL3|Jolt|volk|imgui|onnx|python|torch"
        )
    endif()

    # Each combat mode works without the other (roadmap T39/T40) and without world/physics.
    if(TARGET UmbraTurnCombatTests)
        umbra_add_headless_check(
            Umbra.TurnOnlyDependencies
            TARGETS UmbraTurnCombatTests
            FORBIDDEN "UmbraRealtimeCombat|Umbra(Render|Platform|World|Physics|Gameplay)|SDL3|Jolt|volk|imgui"
        )
    endif()
    if(TARGET UmbraRealtimeCombatTests)
        umbra_add_headless_check(
            Umbra.RealtimeOnlyDependencies
            TARGETS UmbraRealtimeCombatTests
            FORBIDDEN "UmbraTurnCombat|Umbra(Render|Platform|World|Physics|Gameplay)|SDL3|Jolt|volk|imgui"
        )
    endif()
endif()
