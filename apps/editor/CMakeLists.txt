find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

# The editor is its own program; the game never links any of this (Umbra.GameExcludesEditor).
add_executable(UmbraEditor)

target_sources(
    UmbraEditor
    PRIVATE
        Main.cpp
        Editor.cpp
        Editor.hpp
        Editor.md
        RpgForms.cpp
        AiDraft.cpp
        AiTexture.cpp
        AiModel.cpp
        # Shared with the game: model files -> GPU meshes (one copy of the code, two programs).
        ../umbra/ModelLibrary.cpp
        ../umbra/ModelLibrary.hpp
)

target_compile_features(UmbraEditor PRIVATE cxx_std_20)

target_compile_definitions(
    UmbraEditor
    PRIVATE
        UMBRA_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/assets"
        UMBRA_CACHE_DIRECTORY="${CMAKE_BINARY_DIR}/asset-cache"
        UMBRA_GAME_EXECUTABLE="$<TARGET_FILE:Umbra>"
)

target_link_libraries(
    UmbraEditor
    PRIVATE
        Umbra::Core
        Umbra::Diagnostics
        Umbra::Platform
        Umbra::Render
        Umbra::World
        Umbra::Assets
        Umbra::Scene
        Umbra::Authoring
        Umbra::LocalAi
        imgui::imgui
        imguizmo::imguizmo
        SDL3::SDL3
        Umbra::Warnings
        Umbra::Sanitizers
)

# Play starts the game, so build it too.
add_dependencies(UmbraEditor Umbra)

umbra_copy_asan_runtime(UmbraEditor)
