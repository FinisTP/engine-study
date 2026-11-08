find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraGameplayTests)
target_sources(UmbraGameplayTests PRIVATE PhysicsBridgeTests.cpp PlayerControllerTests.cpp SettlementWalkTests.cpp PresentationTests.cpp AreaTests.cpp RoamerTests.cpp BenchmarkTests.cpp ComponentRuntimeTests.cpp ${PROJECT_SOURCE_DIR}/apps/umbra/Benchmark.cpp
    ${PROJECT_SOURCE_DIR}/apps/umbra/GameComponents.cpp ${PROJECT_SOURCE_DIR}/projects/reference/game/Spin.cpp)
target_compile_definitions(UmbraGameplayTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${UMBRA_REFERENCE_ASSETS}")
target_compile_features(UmbraGameplayTests PRIVATE cxx_std_20)
target_include_directories(UmbraGameplayTests PRIVATE ${PROJECT_SOURCE_DIR}/apps/umbra) # Benchmark (P9)
target_link_libraries(
    UmbraGameplayTests
    PRIVATE
        Umbra::Gameplay
        Umbra::Scene
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraGameplayTests)

add_test(NAME Umbra.Gameplay COMMAND UmbraGameplayTests)
