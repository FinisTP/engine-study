add_executable(Umbra)

target_sources(
    Umbra
    PRIVATE
        GameFlow.cpp
        GameFlow.hpp
        GameFlow.md
        Main.cpp
        ModelLibrary.cpp
        ModelLibrary.hpp
        SceneRuntime.cpp
        SceneRuntime.hpp
)

target_compile_features(Umbra PRIVATE cxx_std_20)

target_compile_definitions(
    Umbra
    PRIVATE
        UMBRA_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/assets"
        UMBRA_CACHE_DIRECTORY="${CMAKE_BINARY_DIR}/asset-cache"
)

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
install(DIRECTORY ${PROJECT_SOURCE_DIR}/assets/ DESTINATION assets COMPONENT game)
