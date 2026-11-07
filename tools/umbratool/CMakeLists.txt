find_package(nlohmann_json CONFIG REQUIRED)

add_executable(UmbraTool)
target_sources(UmbraTool PRIVATE Main.cpp README.md)
target_compile_features(UmbraTool PRIVATE cxx_std_20)
target_link_libraries(
    UmbraTool
    PRIVATE
        Umbra::Authoring
        Umbra::TurnCombat
        Umbra::Policy
        nlohmann_json::nlohmann_json
        Umbra::Warnings
        Umbra::Sanitizers
)

umbra_copy_asan_runtime(UmbraTool)
