find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraGameplayTests)
target_sources(UmbraGameplayTests PRIVATE PhysicsBridgeTests.cpp PlayerControllerTests.cpp SettlementWalkTests.cpp PresentationTests.cpp AreaTests.cpp)
target_compile_definitions(UmbraGameplayTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/assets")
target_compile_features(UmbraGameplayTests PRIVATE cxx_std_20)
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
