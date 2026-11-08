find_package(Catch2 3 CONFIG REQUIRED)

# Combined fixture: both modes through the coordinator.
add_executable(UmbraEncounterTests)
target_sources(UmbraEncounterTests PRIVATE EncounterTests.cpp)
target_include_directories(UmbraEncounterTests PRIVATE ${PROJECT_SOURCE_DIR}/tests/rpg)
target_compile_definitions(UmbraEncounterTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${UMBRA_REFERENCE_ASSETS}")
target_compile_features(UmbraEncounterTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraEncounterTests
    PRIVATE
        Umbra::Encounter
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraEncounterTests)

add_test(NAME Umbra.Encounter COMMAND UmbraEncounterTests)
