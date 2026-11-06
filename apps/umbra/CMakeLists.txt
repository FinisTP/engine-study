add_executable(Umbra)

target_sources(
    Umbra
    PRIVATE
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
        Umbra::Warnings
        Umbra::Sanitizers
)

umbra_copy_asan_runtime(Umbra)
