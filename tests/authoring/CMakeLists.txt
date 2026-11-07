find_package(Catch2 3 CONFIG REQUIRED)

# Shared content edits (editor, tool), headless.
add_executable(UmbraAuthoringTests)
target_sources(UmbraAuthoringTests PRIVATE DocumentTests.cpp)
target_include_directories(UmbraAuthoringTests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_compile_definitions(UmbraAuthoringTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/assets")
target_compile_features(UmbraAuthoringTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraAuthoringTests
    PRIVATE
        Umbra::Authoring
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraAuthoringTests)

add_test(NAME Umbra.Authoring COMMAND UmbraAuthoringTests)
