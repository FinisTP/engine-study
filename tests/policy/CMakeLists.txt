find_package(Catch2 3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Learned enemy contract, environment, network and model files, headless.
add_executable(UmbraPolicyTests)
target_sources(UmbraPolicyTests PRIVATE PolicyTests.cpp)
target_include_directories(UmbraPolicyTests PRIVATE ${PROJECT_SOURCE_DIR}/tests/rpg ${PROJECT_SOURCE_DIR}/tests)
target_compile_definitions(UmbraPolicyTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/assets" UMBRA_TEST_DATA_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/data")
target_compile_features(UmbraPolicyTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraPolicyTests
    PRIVATE
        Umbra::Policy
        nlohmann_json::nlohmann_json
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraPolicyTests)

add_test(NAME Umbra.Policy COMMAND UmbraPolicyTests)

# The shipped model must still fit the shipped content (the same check the game runs).
add_test(
    NAME Umbra.Tool.PolicyCheck
    COMMAND UmbraTool policy-check ${PROJECT_SOURCE_DIR}/assets/policies/bandit_leader.policy.json --content ${PROJECT_SOURCE_DIR}/assets/rpg/reference.rpg.json
)
