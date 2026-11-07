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
