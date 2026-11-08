find_package(Catch2 3 CONFIG REQUIRED)

# Real-time-only fixture: links the real-time controller and the shared rules, no turn state (T40).
add_executable(UmbraRealtimeCombatTests)
target_sources(UmbraRealtimeCombatTests PRIVATE RealtimeEncounterTests.cpp)
target_include_directories(UmbraRealtimeCombatTests PRIVATE ${PROJECT_SOURCE_DIR}/plugins/rpg/tests)
target_compile_definitions(UmbraRealtimeCombatTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${UMBRA_REFERENCE_ASSETS}")
target_compile_features(UmbraRealtimeCombatTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraRealtimeCombatTests
    PRIVATE
        Umbra::RealtimeCombat
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraRealtimeCombatTests)

add_test(NAME Umbra.RealtimeCombat COMMAND UmbraRealtimeCombatTests)
