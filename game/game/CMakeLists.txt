find_package(nlohmann_json CONFIG REQUIRED)

add_library(UmbraGame STATIC)
add_library(Umbra::Game ALIAS UmbraGame)

target_sources(
    UmbraGame
    PRIVATE
        src/SaveGame.cpp
        src/GameSession.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/SaveGame.hpp
            include/GameSession.hpp
)

target_compile_features(UmbraGame PUBLIC cxx_std_20)

# The game layer: story, combat and saves together. Still headless.
target_link_libraries(
    UmbraGame
    PUBLIC
        Umbra::Story
        Umbra::Encounter
    PRIVATE
        nlohmann_json::nlohmann_json
        Umbra::Warnings
        Umbra::Sanitizers
)
