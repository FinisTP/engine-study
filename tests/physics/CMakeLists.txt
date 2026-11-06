find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraPhysicsTests)
target_sources(UmbraPhysicsTests PRIVATE PhysicsTests.cpp)
target_compile_features(UmbraPhysicsTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraPhysicsTests
    PRIVATE
        Umbra::Physics
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraPhysicsTests)

add_test(NAME Umbra.Physics COMMAND UmbraPhysicsTests)
