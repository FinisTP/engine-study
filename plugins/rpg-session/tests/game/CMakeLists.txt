find_package(Catch2 3 CONFIG REQUIRED)

# Game layer: saves and the game session, headless.
add_executable(UmbraGameTests)
target_sources(UmbraGameTests PRIVATE SaveGameTests.cpp GameSessionTests.cpp SoakTests.cpp)
target_include_directories(UmbraGameTests PRIVATE ${PROJECT_SOURCE_DIR}/plugins/rpg/tests ${PROJECT_SOURCE_DIR}/tests)
target_compile_definitions(UmbraGameTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${UMBRA_REFERENCE_ASSETS}")
target_compile_features(UmbraGameTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraGameTests
    PRIVATE
        Umbra::Game
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraGameTests)

add_test(NAME Umbra.Game COMMAND UmbraGameTests)
