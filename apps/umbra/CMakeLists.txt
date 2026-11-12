# Android: a shared library named libmain.so, started by SDL's Java activity (SDL_main.h in Main.cpp).
if(ANDROID)
    add_library(Umbra SHARED)
    set_target_properties(Umbra PROPERTIES OUTPUT_NAME main)
else()
    add_executable(Umbra)
endif()

target_sources(
    Umbra
    PRIVATE
        GameFlow.cpp
        GameFeel.cpp
        TownFlow.cpp
        GameFlow.hpp
        GameFlow.md
        Benchmark.cpp
        Benchmark.hpp
        Benchmark.md
        Main.cpp
        ModelLibrary.cpp
        ModelLibrary.hpp
        SceneRuntime.cpp
        SceneRuntime.hpp
        GameComponents.cpp
        GameComponents.hpp
        GameUi.cpp
        GameUi.hpp
        TouchControls.cpp
        TouchControls.hpp
        Mobile.md
)

# The project's own code (projects/<name>/engine and game) is compiled into the program itself,
# so UMBRA_COMPONENT registrations are never dropped by the linker. Only the project name goes in
# (the game finds projects/<name>/assets at run time), never a build path.
file(GLOB_RECURSE UMBRA_PROJECT_CODE CONFIGURE_DEPENDS
     "${UMBRA_PROJECT_DIR}/engine/*.cpp" "${UMBRA_PROJECT_DIR}/engine/*.hpp" "${UMBRA_PROJECT_DIR}/game/*.cpp" "${UMBRA_PROJECT_DIR}/game/*.hpp")
target_sources(Umbra PRIVATE ${UMBRA_PROJECT_CODE})
target_include_directories(Umbra PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} "${UMBRA_PROJECT_DIR}/engine")
target_compile_definitions(Umbra PRIVATE UMBRA_PROJECT_NAME="${UMBRA_PROJECT}" "UMBRA_GAME_PLUGINS=\"${UMBRA_PLUGINS_TEXT}\"")

# Plugins: the game needs the RPG ones and third-person gameplay; combat modes and the learned
# enemy are optional (UMBRA_PLUGIN_* guards). Each plugin's game/ folder compiles in.
umbra_require_plugins("The game" rpg story rpg-session third-person rpg-ui)
umbra_add_plugin_sources(Umbra game)

target_compile_features(Umbra PRIVATE cxx_std_20)

# No build paths are compiled into the game: it finds assets/ at run time (Main.cpp, FindFolders).
# MSVC: the debug-info path in the program is only the file name, not the build folder (P9).
if(MSVC)
    target_link_options(Umbra PRIVATE "/PDBALTPATH:%_PDB%")
endif()

target_link_libraries(
    Umbra
    PRIVATE
        Umbra::Core
        Umbra::Diagnostics
        Umbra::Platform
        Umbra::Render
        Umbra::World
        Umbra::Assets
        Umbra::Scene
        Umbra::Ui
        Umbra::Gameplay
        Umbra::Audio
        Umbra::Game
        $<TARGET_NAME_IF_EXISTS:Umbra::Policy>
        $<TARGET_NAME_IF_EXISTS:Umbra::Town>
        $<TARGET_NAME_IF_EXISTS:Umbra::TownMind>
        $<TARGET_NAME_IF_EXISTS:Umbra::TownStory>
        $<TARGET_NAME_IF_EXISTS:Umbra::TownLlama>
        Umbra::Warnings
        Umbra::Sanitizers
)

umbra_copy_asan_runtime(Umbra)

# Android: SDL is linked statically, so keep its JNI_OnLoad (it registers SDL's Java callbacks)
# and export SDL_main, which the activity looks up by name. No install rules: Gradle packs the APK.
if(ANDROID)
    target_link_options(Umbra PRIVATE "LINKER:--undefined=JNI_OnLoad")
    target_link_libraries(Umbra PRIVATE android log)
    return()
endif()

# Browser build (roadmap P10, plugins/web-export/web/README.md): umbra.js/.wasm plus the page, the offline service
# worker and its version stamp. The assets are not linked in: "UmbraTool web-pack" writes
# assets.bin/assets.json next to the page (plugins/web-export/tools/build-web.sh, scripts/serve-web.ps1).
# Single-threaded; the frame loop yields to the browser
# through Asyncify (RenderFrame waits for the next animation frame).
if(EMSCRIPTEN)
    set_target_properties(Umbra PROPERTIES OUTPUT_NAME umbra SUFFIX ".js")
    target_link_options(
        Umbra PRIVATE
        -sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 -sSTACK_SIZE=1048576
        -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728
        -sFORCE_FILESYSTEM=1 -lidbfs.js -sEXPORTED_RUNTIME_METHODS=FS,IDBFS -sENVIRONMENT=web -sDYNCALLS=1 # SDL audio calls back through dynCall
    )
    # Relink when the JavaScript renderer changes.
    set_property(TARGET Umbra APPEND PROPERTY LINK_DEPENDS "${PROJECT_SOURCE_DIR}/engine/render/web/umbra_render.js")
    set(UMBRA_WEB_PAGE "${UMBRA_PLUGIN_DIR_web-export}/web") # the web-export plugin
    add_custom_target(
        UmbraWebPage ALL
        DEPENDS Umbra
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${UMBRA_WEB_PAGE}/index.html" "${UMBRA_WEB_PAGE}/umbra-page.js" "${UMBRA_WEB_PAGE}/umbra-page.css" $<TARGET_FILE_DIR:Umbra>
        COMMAND ${CMAKE_COMMAND} -DDIR=$<TARGET_FILE_DIR:Umbra> -DTEMPLATE=${UMBRA_WEB_PAGE}/sw.js -P ${UMBRA_WEB_PAGE}/stamp.cmake
        VERBATIM
    )
    return()
endif()

# Package layout (roadmap P7.5/P7.6): the game, its DLLs, the MSVC runtime, assets/ and shaders/
# side by side (the project's assets folder becomes assets/). The game finds assets/ next to
# itself first (Main.cpp, FindFolders).
install(TARGETS Umbra RUNTIME DESTINATION . COMPONENT game)
if(WIN32)
    install(FILES $<TARGET_RUNTIME_DLLS:Umbra> DESTINATION . COMPONENT game)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT game)
    include(InstallRequiredSystemLibraries)
endif()
# Plugin assets first, then the project's on top (a project file replaces a plugin's).
# AI concept pictures stay in the project; provenance files (.source.json) ship with their models.
foreach(dir ${UMBRA_PLUGIN_ASSETS} ${UMBRA_ASSETS})
    install(DIRECTORY ${dir}/ DESTINATION assets COMPONENT game PATTERN "*.concept.png" EXCLUDE)
endforeach()

# Licenses (P9): the notices file plus the full license text of each library the game links,
# taken from vcpkg (every port installs share/<port>/copyright).
install(FILES ${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md DESTINATION . COMPONENT game)
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    foreach(port sdl3 spdlog fmt joltphysics imgui glm nlohmann-json tinygltf stb volk vulkan-memory-allocator)
        set(copyright "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/${port}/copyright")
        if(EXISTS "${copyright}")
            install(FILES "${copyright}" DESTINATION licenses RENAME "${port}.txt" COMPONENT game)
        endif()
    endforeach()
endif()
