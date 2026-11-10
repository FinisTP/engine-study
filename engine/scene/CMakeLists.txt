find_package(nlohmann_json CONFIG REQUIRED)

add_library(UmbraScene STATIC)
add_library(Umbra::Scene ALIAS UmbraScene)

target_sources(
    UmbraScene
    PRIVATE
        src/ModelInstance.cpp
        src/SceneFile.cpp
        src/Checkpoint.cpp
        src/ComponentSchema.cpp
        src/AnimGraph.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/ModelInstance.hpp
            include/SceneFile.hpp
            include/Checkpoint.hpp
            include/ComponentSchema.hpp
            include/AnimGraph.hpp
            include/AnimGraph.md
)

target_compile_features(UmbraScene PUBLIC cxx_std_20)

target_link_libraries(
    UmbraScene
    PUBLIC
        Umbra::World
        Umbra::Assets
        nlohmann_json::nlohmann_json
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
