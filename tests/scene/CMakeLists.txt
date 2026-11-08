find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraSceneTests)
target_sources(UmbraSceneTests PRIVATE ModelInstanceTests.cpp SceneFileTests.cpp ComponentTests.cpp)
target_compile_definitions(UmbraSceneTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${UMBRA_REFERENCE_ASSETS}")
target_compile_features(UmbraSceneTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraSceneTests
    PRIVATE
        Umbra::Scene
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraSceneTests)

add_test(NAME Umbra.Scene COMMAND UmbraSceneTests)

# The headless validator must accept the shipped sample scene.
add_test(
    NAME Umbra.SceneCheck.Sample
    COMMAND UmbraSceneCheck ${UMBRA_REFERENCE_ASSETS}/scenes/sample.scene.json --assets ${UMBRA_REFERENCE_ASSETS}
)

add_test(
    NAME Umbra.SceneCheck.Settlement
    COMMAND UmbraSceneCheck ${UMBRA_REFERENCE_ASSETS}/scenes/settlement.scene.json --assets ${UMBRA_REFERENCE_ASSETS}
)

add_test(
    NAME Umbra.SceneCheck.Dungeon
    COMMAND UmbraSceneCheck ${UMBRA_REFERENCE_ASSETS}/scenes/dungeon.scene.json --assets ${UMBRA_REFERENCE_ASSETS}
)

add_test(
    NAME Umbra.SceneCheck.Arena
    COMMAND UmbraSceneCheck ${UMBRA_REFERENCE_ASSETS}/scenes/arena.scene.json --assets ${UMBRA_REFERENCE_ASSETS}
)

# A broken scene must make the validator exit with an error.
add_test(
    NAME Umbra.SceneCheck.Broken
    COMMAND UmbraSceneCheck ${CMAKE_CURRENT_SOURCE_DIR}/data/broken.scene.json --assets ${UMBRA_REFERENCE_ASSETS} --json
)
set_tests_properties(Umbra.SceneCheck.Broken PROPERTIES WILL_FAIL TRUE)
