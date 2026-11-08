add_library(UmbraEncounter STATIC)
add_library(Umbra::Encounter ALIAS UmbraEncounter)

target_sources(
    UmbraEncounter
    PRIVATE
        src/EncounterCoordinator.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/EncounterCoordinator.hpp
)

target_compile_features(UmbraEncounter PUBLIC cxx_std_20)

# The only place that knows both modes. Games using one mode link that controller alone.
target_link_libraries(
    UmbraEncounter
    PUBLIC
        Umbra::Rpg
        $<TARGET_NAME_IF_EXISTS:Umbra::TurnCombat>     # each mode is an optional plugin
        $<TARGET_NAME_IF_EXISTS:Umbra::RealtimeCombat>
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
