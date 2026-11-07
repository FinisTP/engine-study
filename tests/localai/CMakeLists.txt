find_package(Catch2 3 CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

# The Ollama bridge against a stand-in server on 127.0.0.1 (no real model needed).
add_executable(UmbraLocalAiTests)
target_sources(UmbraLocalAiTests PRIVATE OllamaTests.cpp ComfyUiTests.cpp)
target_compile_definitions(UmbraLocalAiTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/assets" UMBRA_TEST_DATA_DIRECTORY="${PROJECT_SOURCE_DIR}/tests/authoring/data")
target_compile_features(UmbraLocalAiTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraLocalAiTests
    PRIVATE
        Umbra::LocalAi
        httplib::httplib
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraLocalAiTests)

add_test(NAME Umbra.LocalAi COMMAND UmbraLocalAiTests)
