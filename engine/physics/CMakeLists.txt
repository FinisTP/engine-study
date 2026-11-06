find_package(Jolt CONFIG REQUIRED)

add_library(UmbraPhysics STATIC)
add_library(Umbra::Physics ALIAS UmbraPhysics)

target_sources(
    UmbraPhysics
    PRIVATE
        src/Physics.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Physics.hpp
)

target_compile_features(UmbraPhysics PUBLIC cxx_std_20)

target_link_libraries(
    UmbraPhysics
    PUBLIC
        Umbra::Core
    PRIVATE
        Jolt::Jolt
        Umbra::Warnings
        Umbra::Sanitizers
)
