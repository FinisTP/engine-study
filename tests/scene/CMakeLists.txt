find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraSceneTests)
target_sources(UmbraSceneTests PRIVATE ModelInstanceTests.cpp SceneFileTests.cpp)
target_compile_definitions(UmbraSceneTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/assets")
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
    COMMAND UmbraSceneCheck ${PROJECT_SOURCE_DIR}/assets/scenes/sample.scene.json --assets ${PROJECT_SOURCE_DIR}/assets
)
