# These suites exercise both combat modes (mode switches, both fight kinds, long mixed runs), so
# they run in projects that have both combat plugins, like the reference project.
if(TARGET Umbra::TurnCombat AND TARGET Umbra::RealtimeCombat)
    add_subdirectory(encounter)
    add_subdirectory(game)
endif()
