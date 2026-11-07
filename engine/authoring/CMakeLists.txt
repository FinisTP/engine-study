find_package(nlohmann_json CONFIG REQUIRED)

add_library(UmbraAuthoring STATIC)
add_library(Umbra::Authoring ALIAS UmbraAuthoring)

target_sources(
    UmbraAuthoring
    PRIVATE
        src/Document.cpp
        src/SceneEdits.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Document.hpp
            include/SceneEdits.hpp
)

target_compile_features(UmbraAuthoring PUBLIC cxx_std_20)

# The one edit path for content: editor, command-line tool and (later) AI authoring tools.
# Headless; the game itself never links it.
target_link_libraries(
    UmbraAuthoring
    PUBLIC
        Umbra::Scene
        Umbra::Rpg
        nlohmann_json::nlohmann_json
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
