find_package(Catch2 3 CONFIG REQUIRED)

# Shared content edits (editor, tool), headless.
add_executable(UmbraAuthoringTests)
target_sources(UmbraAuthoringTests PRIVATE DocumentTests.cpp ValidateTests.cpp)
target_include_directories(UmbraAuthoringTests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_compile_definitions(UmbraAuthoringTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/assets" UMBRA_TEST_DATA_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/data")
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

# The command-line tool on shipped content (must pass) and on a planted error (must fail).
add_test(
    NAME Umbra.Tool.ValidateShipped
    COMMAND UmbraTool validate
        ${PROJECT_SOURCE_DIR}/assets/scenes/settlement.scene.json
        ${PROJECT_SOURCE_DIR}/assets/scenes/dungeon.scene.json
        ${PROJECT_SOURCE_DIR}/assets/scenes/arena.scene.json
        ${PROJECT_SOURCE_DIR}/assets/rpg/reference.rpg.json
        --assets ${PROJECT_SOURCE_DIR}/assets
)
add_test(
    NAME Umbra.Tool.ValidateBroken
    COMMAND UmbraTool validate ${CMAKE_CURRENT_SOURCE_DIR}/data/broken.rpg.json --json
)
set_tests_properties(Umbra.Tool.ValidateBroken PROPERTIES WILL_FAIL TRUE)
add_test(
    NAME Umbra.Tool.Fight
    COMMAND UmbraTool fight ${PROJECT_SOURCE_DIR}/assets/rpg/reference.rpg.json hero wolf
)
set_tests_properties(Umbra.Tool.Fight PROPERTIES PASS_REGULAR_EXPRESSION "Hero side wins")
