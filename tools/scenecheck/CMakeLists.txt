add_executable(UmbraSceneCheck)
target_sources(UmbraSceneCheck PRIVATE Main.cpp)
target_compile_features(UmbraSceneCheck PRIVATE cxx_std_20)
target_link_libraries(
    UmbraSceneCheck
    PRIVATE
        Umbra::Scene
        Umbra::Assets
        Umbra::Warnings
        Umbra::Sanitizers
)

umbra_copy_asan_runtime(UmbraSceneCheck)
