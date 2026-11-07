find_package(Catch2 3 CONFIG REQUIRED)

# Shared rules only: no combat controller, no world, no renderer.
add_executable(UmbraRpgTests)
target_sources(UmbraRpgTests PRIVATE ContentTests.cpp)
target_compile_definitions(UmbraRpgTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/assets")
target_compile_features(UmbraRpgTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraRpgTests
    PRIVATE
        Umbra::Rpg
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraRpgTests)

add_test(NAME Umbra.Rpg COMMAND UmbraRpgTests)
