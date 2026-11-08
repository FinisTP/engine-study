find_package(Catch2 3 CONFIG REQUIRED)

# The game UI framework (S3): fonts, screen files, layout, bindings and input, without a window.
add_executable(UmbraUiTests)
target_sources(UmbraUiTests PRIVATE UiTests.cpp)
target_include_directories(UmbraUiTests PRIVATE ${PROJECT_SOURCE_DIR}/tests)
target_compile_definitions(UmbraUiTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${UMBRA_REFERENCE_ASSETS}" UMBRA_RPG_UI_ASSETS="${PROJECT_SOURCE_DIR}/plugins/rpg-ui/assets")
target_compile_features(UmbraUiTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraUiTests
    PRIVATE
        Umbra::Ui
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraUiTests)

add_test(NAME Umbra.Ui COMMAND UmbraUiTests)

# A screen drawn to a picture through the tool, with the plugin's sample data (no window).
if(TARGET UmbraTool)
    add_test(
        NAME Umbra.Tool.UiPreview
        COMMAND UmbraTool ui-preview hud inventory --assets ${UMBRA_REFERENCE_ASSETS} --out ${CMAKE_CURRENT_BINARY_DIR}/inventory.png
    )
    set_tests_properties(Umbra.Tool.UiPreview PROPERTIES PASS_REGULAR_EXPRESSION "0 error")
endif()
