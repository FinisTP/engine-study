find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraWorldTests)
target_sources(UmbraWorldTests PRIVATE WorldTests.cpp TransformTests.cpp)
target_compile_features(UmbraWorldTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraWorldTests
    PRIVATE
        Umbra::World
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraWorldTests)

add_test(NAME Umbra.World COMMAND UmbraWorldTests)
