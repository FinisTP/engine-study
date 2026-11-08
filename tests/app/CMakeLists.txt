find_package(Catch2 3 CONFIG REQUIRED)

# Parts of the game program that work without a window: benchmark statistics (P9) and project
# components (S1: the reference project's Spin is compiled in, as in the game), touch controls (P11).
add_executable(UmbraAppTests)
target_sources(UmbraAppTests PRIVATE BenchmarkTests.cpp ComponentRuntimeTests.cpp TouchTests.cpp ${PROJECT_SOURCE_DIR}/apps/umbra/Benchmark.cpp
               ${PROJECT_SOURCE_DIR}/apps/umbra/TouchControls.cpp
               ${PROJECT_SOURCE_DIR}/apps/umbra/GameComponents.cpp ${PROJECT_SOURCE_DIR}/projects/reference/game/Spin.cpp)
target_compile_definitions(UmbraAppTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${UMBRA_REFERENCE_ASSETS}")
target_compile_features(UmbraAppTests PRIVATE cxx_std_20)
target_include_directories(UmbraAppTests PRIVATE ${PROJECT_SOURCE_DIR}/apps/umbra)
target_link_libraries(
    UmbraAppTests
    PRIVATE
        Umbra::Scene
        Umbra::Gameplay
        Umbra::Ui
        Umbra::Platform
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraAppTests)

add_test(NAME Umbra.App COMMAND UmbraAppTests)
