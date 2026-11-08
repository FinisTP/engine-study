find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraAudioTests)
target_sources(UmbraAudioTests PRIVATE AudioTests.cpp)
target_compile_definitions(UmbraAudioTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${UMBRA_REFERENCE_ASSETS}" UMBRA_AUDIO_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_include_directories(UmbraAudioTests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_compile_features(UmbraAudioTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraAudioTests
    PRIVATE
        Umbra::Audio
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraAudioTests)

add_test(NAME Umbra.Audio COMMAND UmbraAudioTests)
