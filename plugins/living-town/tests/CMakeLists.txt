find_package(Catch2 3 CONFIG REQUIRED)

# The town simulation, headless (S21).
add_executable(UmbraTownTests)
target_sources(UmbraTownTests PRIVATE TownTests.cpp MindTests.cpp StoryTests.cpp)
target_compile_definitions(UmbraTownTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${UMBRA_REFERENCE_ASSETS}")
target_compile_features(UmbraTownTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraTownTests
    PRIVATE
        Umbra::Town
        Umbra::TownMind
        Umbra::TownStory
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraTownTests)

add_test(NAME Umbra.Town COMMAND UmbraTownTests)
