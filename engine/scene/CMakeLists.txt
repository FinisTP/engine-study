find_package(nlohmann_json CONFIG REQUIRED)

add_library(UmbraScene STATIC)
add_library(Umbra::Scene ALIAS UmbraScene)

target_sources(
    UmbraScene
    PRIVATE
        src/ModelInstance.cpp
        src/SceneFile.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/ModelInstance.hpp
            include/SceneFile.hpp
)

target_compile_features(UmbraScene PUBLIC cxx_std_20)

target_link_libraries(
    UmbraScene
    PUBLIC
        Umbra::World
        Umbra::Assets
    PRIVATE
        nlohmann_json::nlohmann_json
        Umbra::Warnings
        Umbra::Sanitizers
)
