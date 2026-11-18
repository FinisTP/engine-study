add_library(UmbraMatch STATIC)
add_library(Umbra::Match ALIAS UmbraMatch)

target_sources(
    UmbraMatch
    PRIVATE
        src/MatchData.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/MatchDef.hpp
            include/Match.md
)

target_compile_features(UmbraMatch PUBLIC cxx_std_20)

# The faction match: rules and data only, no renderer, window or model (docs/design/2026-11-19-factions.md).
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(
    UmbraMatch
    PUBLIC
        Umbra::Core
        nlohmann_json::nlohmann_json
    PRIVATE
        Umbra::Night
        Umbra::Warnings
        Umbra::Sanitizers
)
