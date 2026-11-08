find_package(Catch2 3 CONFIG REQUIRED)

# Turn-only fixture: links the turn controller and the shared rules, nothing real-time (T39).
add_executable(UmbraTurnCombatTests)
target_sources(UmbraTurnCombatTests PRIVATE TurnEncounterTests.cpp TurnEnvironmentTests.cpp)
target_include_directories(UmbraTurnCombatTests PRIVATE ${PROJECT_SOURCE_DIR}/tests/rpg)
target_compile_definitions(UmbraTurnCombatTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${UMBRA_REFERENCE_ASSETS}")
target_compile_features(UmbraTurnCombatTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraTurnCombatTests
    PRIVATE
        Umbra::TurnCombat
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraTurnCombatTests)

add_test(NAME Umbra.TurnCombat COMMAND UmbraTurnCombatTests)
