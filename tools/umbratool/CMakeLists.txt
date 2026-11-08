find_package(nlohmann_json CONFIG REQUIRED)

add_executable(UmbraTool)
target_sources(UmbraTool PRIVATE Main.cpp ToolCommand.cpp ToolCommand.hpp UiCommand.cpp README.md)
target_compile_features(UmbraTool PRIVATE cxx_std_20)
target_include_directories(UmbraTool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}) # ToolCommand.hpp for plugin commands
target_compile_definitions(UmbraTool PRIVATE "UMBRA_PLUGIN_ASSET_DIRS=\"${UMBRA_PLUGIN_ASSETS_TEXT}\"")
target_link_libraries(
    UmbraTool
    PRIVATE
        Umbra::Authoring
        Umbra::Ui
        nlohmann_json::nlohmann_json
        Umbra::Warnings
        Umbra::Sanitizers
)
# Commands from plugins (each plugin's tool/ folder): fight, policy-check, draft, texture, model...
umbra_require_plugins("UmbraTool" rpg story rpg-session)
umbra_add_plugin_sources(UmbraTool tool)

umbra_copy_asan_runtime(UmbraTool)
