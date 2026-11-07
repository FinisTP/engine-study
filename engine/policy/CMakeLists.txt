find_package(nlohmann_json CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(UmbraPolicy STATIC)
add_library(Umbra::Policy ALIAS UmbraPolicy)

target_sources(
    UmbraPolicy
    PRIVATE
        src/PolicyContract.cpp
        src/PolicyEnvironment.cpp
        src/PolicyModel.cpp
        src/PolicyDecider.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/PolicyContract.hpp
            include/PolicyEnvironment.hpp
            include/PolicyModel.hpp
            include/PolicyDecider.hpp
)

target_compile_features(UmbraPolicy PUBLIC cxx_std_20)

# Learned turn-based enemy: contract, training environment, network, model files. Headless and
# turn-only: no real-time controller, no Python, no ML runtime (the network is plain C++).
target_link_libraries(
    UmbraPolicy
    PUBLIC
        Umbra::TurnCombat
        Threads::Threads
    PRIVATE
        nlohmann_json::nlohmann_json
        Umbra::Warnings
        Umbra::Sanitizers
)
