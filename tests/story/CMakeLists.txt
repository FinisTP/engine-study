find_package(Catch2 3 CONFIG REQUIRED)

# Quests and conversations, headless.
add_executable(UmbraStoryTests)
target_sources(UmbraStoryTests PRIVATE ConversationTests.cpp QuestTests.cpp)
target_include_directories(UmbraStoryTests PRIVATE ${PROJECT_SOURCE_DIR}/tests/rpg)
target_compile_definitions(UmbraStoryTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/assets")
target_compile_features(UmbraStoryTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraStoryTests
    PRIVATE
        Umbra::Story
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraStoryTests)

add_test(NAME Umbra.Story COMMAND UmbraStoryTests)
