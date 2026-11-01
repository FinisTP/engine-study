add_library(UmbraWorld STATIC)
add_library(Umbra::World ALIAS UmbraWorld)

target_sources(
    UmbraWorld
    PRIVATE
        src/World.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/World.hpp
)

target_compile_features(UmbraWorld PUBLIC cxx_std_20)

target_link_libraries(
    UmbraWorld
    PUBLIC
        Umbra::Core
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
