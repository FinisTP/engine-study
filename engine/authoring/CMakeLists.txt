find_package(nlohmann_json CONFIG REQUIRED)

add_library(UmbraAuthoring STATIC)
add_library(Umbra::Authoring ALIAS UmbraAuthoring)

target_sources(
    UmbraAuthoring
    PRIVATE
        src/Document.cpp
        src/Package.cpp
        src/WebPack.cpp
        src/ModelTools.cpp
        src/SceneEdits.cpp
        src/Validate.cpp
        src/Play.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Document.hpp
            include/Package.hpp
            include/WebPack.hpp
            include/ModelTools.hpp
            include/SceneEdits.hpp
            include/Validate.hpp
            include/Play.hpp
)

target_compile_features(UmbraAuthoring PUBLIC cxx_std_20)

# The one edit path for content: editor, command-line tool and (later) AI authoring tools.
# Headless; the game itself never links it.
target_link_libraries(
    UmbraAuthoring
    PUBLIC
        Umbra::Scene
        Umbra::Rpg
        Umbra::Game
        Umbra::Assets
        nlohmann_json::nlohmann_json
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
