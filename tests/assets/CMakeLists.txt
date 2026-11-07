find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraAssetsTests)
target_sources(UmbraAssetsTests PRIVATE GltfImporterTests.cpp ModelCacheTests.cpp ReloadTests.cpp TextureToolsTests.cpp TestFiles.hpp)
target_compile_features(UmbraAssetsTests PRIVATE cxx_std_20)
target_compile_definitions(UmbraAssetsTests PRIVATE UMBRA_TEST_ASSET_DIRECTORY="${PROJECT_SOURCE_DIR}/assets")
target_link_libraries(
    UmbraAssetsTests
    PRIVATE
        Umbra::Assets
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraAssetsTests)

add_test(NAME Umbra.Assets COMMAND UmbraAssetsTests)
