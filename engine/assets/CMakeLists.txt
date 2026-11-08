find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h" REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(UmbraAssets STATIC)
add_library(Umbra::Assets ALIAS UmbraAssets)

target_sources(
    UmbraAssets
    PRIVATE
        src/GltfImporter.cpp
        src/ModelCache.cpp
        src/AssetFolders.cpp
        src/FileWatcher.cpp
        src/Material.cpp
        src/GamePack.cpp
        src/TextureTools.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Model.hpp
            include/GltfImporter.hpp
            include/ModelCache.hpp
            include/AssetFolders.hpp
            include/FileWatcher.hpp
            include/Material.hpp
            include/GamePack.hpp
            include/TextureTools.hpp
)

target_compile_features(UmbraAssets PUBLIC cxx_std_20)
target_include_directories(UmbraAssets PRIVATE ${TINYGLTF_INCLUDE_DIRS})

target_link_libraries(
    UmbraAssets
    PUBLIC
        Umbra::Core
    PRIVATE
        nlohmann_json::nlohmann_json
        Umbra::Warnings
        Umbra::Sanitizers
)
