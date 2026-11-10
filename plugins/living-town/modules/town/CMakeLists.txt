add_library(UmbraTown STATIC)
add_library(Umbra::Town ALIAS UmbraTown)

target_sources(
    UmbraTown
    PRIVATE
        src/TownDef.cpp
        src/Town.cpp
        src/TownSave.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/TownDef.hpp
            include/Town.hpp
)

target_compile_features(UmbraTown PUBLIC cxx_std_20)

# The town simulation: rules only, no world, physics, renderer or model (ADR 0011).
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(
    UmbraTown
    PUBLIC
        Umbra::Rpg
        nlohmann_json::nlohmann_json
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
