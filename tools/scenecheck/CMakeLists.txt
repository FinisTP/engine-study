find_package(nlohmann_json CONFIG REQUIRED)

add_executable(UmbraSceneCheck)
target_sources(UmbraSceneCheck PRIVATE Main.cpp)
target_compile_features(UmbraSceneCheck PRIVATE cxx_std_20)
target_compile_definitions(UmbraSceneCheck PRIVATE "UMBRA_PLUGIN_ASSET_DIRS=\"${UMBRA_PLUGIN_ASSETS_TEXT}\"")
target_link_libraries(
    UmbraSceneCheck
    PRIVATE
        Umbra::Scene
        Umbra::Assets
        nlohmann_json::nlohmann_json
        Umbra::Warnings
        Umbra::Sanitizers
)

umbra_copy_asan_runtime(UmbraSceneCheck)
