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
        EditorPanel.cpp
        EditorPanel.hpp
        Components.cpp
        CodePanel.cpp
        UiPanel.cpp
        Widgets.hpp
        # Shared with the game: model files -> GPU meshes (one copy of the code, two programs).
        ../umbra/ModelLibrary.cpp
        ../umbra/ModelLibrary.hpp
)

target_compile_features(UmbraEditor PRIVATE cxx_std_20)

target_compile_definitions(
    UmbraEditor
    PRIVATE
        UMBRA_ASSET_DIRECTORY="${UMBRA_ASSETS}"
        UMBRA_SOURCE_DIRECTORY="${PROJECT_SOURCE_DIR}"
        UMBRA_PROJECT_NAME="${UMBRA_PROJECT}"
        UMBRA_PROJECT_DIRECTORY="${UMBRA_PROJECT_DIR}"
        UMBRA_CACHE_DIRECTORY="${CMAKE_BINARY_DIR}/asset-cache"
        UMBRA_GAME_EXECUTABLE="$<TARGET_FILE:Umbra>"
        "UMBRA_PLUGIN_ASSET_DIRS=\"${UMBRA_PLUGIN_ASSETS_TEXT}\""
        "UMBRA_PLUGIN_SOURCE_DIRS=\"${UMBRA_PLUGIN_DIRS_TEXT}\""
)
target_include_directories(UmbraEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}) # EditorPanel.hpp for plugin panels

# Plugin windows (each plugin's editor/ folder) compile into the editor; it needs the RPG plugins
# for its content checks and the RPG pickers in the inspector.
umbra_require_plugins("The editor" rpg story rpg-session)
umbra_add_plugin_sources(UmbraEditor editor)

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
        Umbra::Ui
        Umbra::Audio
        imgui::imgui
        imguizmo::imguizmo
        SDL3::SDL3
        Umbra::Warnings
        Umbra::Sanitizers
)

# Play starts the game, so build it too.
add_dependencies(UmbraEditor Umbra)

umbra_copy_asan_runtime(UmbraEditor)
