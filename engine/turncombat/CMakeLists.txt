add_library(UmbraTurnCombat STATIC)
add_library(Umbra::TurnCombat ALIAS UmbraTurnCombat)

target_sources(
    UmbraTurnCombat
    PRIVATE
        src/TurnEncounter.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/TurnEncounter.hpp
)

target_compile_features(UmbraTurnCombat PUBLIC cxx_std_20)

# Only the shared rules: no real-time controller, world, physics or renderer (roadmap T39).
target_link_libraries(
    UmbraTurnCombat
    PUBLIC
        Umbra::Rpg
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
