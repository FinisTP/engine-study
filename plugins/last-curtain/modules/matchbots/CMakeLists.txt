add_library(UmbraMatchBots STATIC)
add_library(Umbra::MatchBots ALIAS UmbraMatchBots)

target_sources(
    UmbraMatchBots
    PRIVATE
        src/MatchBots.cpp
        src/MatchSoak.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/MatchBots.hpp
            include/MatchSoak.hpp
            include/Bots.md
)

target_compile_features(UmbraMatchBots PUBLIC cxx_std_20)

# The faction match's bots and soak: headless, built on the rules only.
target_link_libraries(
    UmbraMatchBots
    PUBLIC
        Umbra::Match
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)

target_compile_definitions(UmbraMatchBots PUBLIC UMBRA_MATCH_DEFAULT_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../../assets/match")
