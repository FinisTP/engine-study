add_library(UmbraScene STATIC)
add_library(Umbra::Scene ALIAS UmbraScene)

target_sources(
    UmbraScene
    PRIVATE
        src/ModelInstance.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/ModelInstance.hpp
)

target_compile_features(UmbraScene PUBLIC cxx_std_20)

target_link_libraries(
    UmbraScene
    PUBLIC
        Umbra::World
        Umbra::Assets
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
