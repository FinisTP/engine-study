find_package(Catch2 3 CONFIG REQUIRED)

# Shared content edits (editor, tool), headless.
add_executable(UmbraAuthoringTests)
target_sources(UmbraAuthoringTests PRIVATE DocumentTests.cpp ModelToolsTests.cpp PackageTests.cpp ValidateTests.cpp WebPackTests.cpp PlayTests.cpp)
target_include_directories(UmbraAuthoringTests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_compile_definitions(UmbraAuthoringTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${UMBRA_REFERENCE_ASSETS}" UMBRA_TEST_DATA_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/data"
                                                        UMBRA_TEST_PROJECT_FILE="${UMBRA_PROJECT_DIR}/project.json" UMBRA_TEST_PROGRAMS_FILE="${UMBRA_PROGRAMS_FILE}")
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
        ${UMBRA_REFERENCE_ASSETS}/scenes/settlement.scene.json
        ${UMBRA_REFERENCE_ASSETS}/scenes/dungeon.scene.json
        ${UMBRA_REFERENCE_ASSETS}/scenes/arena.scene.json
        ${UMBRA_REFERENCE_ASSETS}/rpg/reference.rpg.json
        --assets ${UMBRA_REFERENCE_ASSETS}
)
add_test(
    NAME Umbra.Tool.ValidateBroken
    COMMAND UmbraTool validate ${CMAKE_CURRENT_SOURCE_DIR}/data/broken.rpg.json --json
)
set_tests_properties(Umbra.Tool.ValidateBroken PROPERTIES WILL_FAIL TRUE)


# Shipped materials validate (image size, license warning).
add_test(
    NAME Umbra.Tool.ValidateMaterial
    COMMAND UmbraTool validate ${UMBRA_REFERENCE_ASSETS}/materials/sample_bricks.material.json --assets ${UMBRA_REFERENCE_ASSETS}
)


# The package check through the tool: a folder that isn't a package fails (P9).
add_test(
    NAME Umbra.Tool.PackageCheckRefusesNonPackage
    COMMAND UmbraTool package-check ${CMAKE_CURRENT_SOURCE_DIR}/data
)
set_tests_properties(Umbra.Tool.PackageCheckRefusesNonPackage PROPERTIES PASS_REGULAR_EXPRESSION "package.program")
