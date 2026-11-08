add_library(UmbraRealtimeCombat STATIC)
add_library(Umbra::RealtimeCombat ALIAS UmbraRealtimeCombat)

target_sources(
    UmbraRealtimeCombat
    PRIVATE
        src/RealtimeEncounter.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/RealtimeEncounter.hpp
)

target_compile_features(UmbraRealtimeCombat PUBLIC cxx_std_20)

# Only the shared rules: no turn controller, world, physics or renderer (roadmap T40).
target_link_libraries(
    UmbraRealtimeCombat
    PUBLIC
        Umbra::Rpg
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
