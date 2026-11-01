find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h" REQUIRED)

add_library(UmbraAssets STATIC)
add_library(Umbra::Assets ALIAS UmbraAssets)

target_sources(
    UmbraAssets
    PRIVATE
        src/GltfImporter.cpp
        src/ModelCache.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Model.hpp
            include/GltfImporter.hpp
            include/ModelCache.hpp
)

target_compile_features(UmbraAssets PUBLIC cxx_std_20)
target_include_directories(UmbraAssets PRIVATE ${TINYGLTF_INCLUDE_DIRS})

target_link_libraries(
    UmbraAssets
    PUBLIC
        Umbra::Core
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
