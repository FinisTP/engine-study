add_executable(Umbra)

target_sources(
    Umbra
    PRIVATE
        GameFlow.cpp
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
)

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
        Umbra::Gameplay
        Umbra::Audio
        Umbra::Game
        Umbra::Policy
        Umbra::Warnings
        Umbra::Sanitizers
)

umbra_copy_asan_runtime(Umbra)

# Browser build (roadmap P10, web/README.md): umbra.js/.wasm/.data plus the page, the offline
# service worker and its version stamp. Single-threaded; the frame loop yields to the browser
# through Asyncify (RenderFrame waits for the next animation frame).
if(EMSCRIPTEN)
    set_target_properties(Umbra PROPERTIES OUTPUT_NAME umbra SUFFIX ".js")
    target_link_options(
        Umbra PRIVATE
        -sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 -sSTACK_SIZE=1048576
        -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728
        -sFORCE_FILESYSTEM=1 -lidbfs.js -sEXPORTED_RUNTIME_METHODS=FS,IDBFS -sENVIRONMENT=web -sDYNCALLS=1 # SDL audio calls back through dynCall
        "--preload-file=${PROJECT_SOURCE_DIR}/assets@/assets"
        "--exclude-file=*.concept.png" "--exclude-file=*comfy-export*" "--exclude-file=*.api.json"
    )
    # Relink when the JavaScript renderer or any preloaded asset changes.
    file(GLOB_RECURSE UMBRA_WEB_ASSETS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/*")
    set_property(TARGET Umbra APPEND PROPERTY LINK_DEPENDS "${PROJECT_SOURCE_DIR}/engine/render/web/umbra_render.js" ${UMBRA_WEB_ASSETS})
    set(UMBRA_WEB_PAGE "${CMAKE_CURRENT_SOURCE_DIR}/web")
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
# side by side. The game finds assets/ next to itself first (Main.cpp, FindFolders).
install(TARGETS Umbra RUNTIME DESTINATION . COMPONENT game)
if(WIN32)
    install(FILES $<TARGET_RUNTIME_DLLS:Umbra> DESTINATION . COMPONENT game)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT game)
    include(InstallRequiredSystemLibraries)
endif()
# AI concept pictures stay in the project; provenance files (.source.json) ship with their models.
install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets/ DESTINATION assets COMPONENT game PATTERN "*.concept.png" EXCLUDE)

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
