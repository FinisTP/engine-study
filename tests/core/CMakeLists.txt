find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraCoreTests)

target_sources(
    UmbraCoreTests
    PRIVATE
        VersionTests.cpp
        FrameClockTests.cpp
        FixedStepAccumulatorTests.cpp
)

target_compile_features(UmbraCoreTests PRIVATE cxx_std_20)

target_link_libraries(
    UmbraCoreTests
    PRIVATE
        Umbra::Core
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraCoreTests)

add_test(
    NAME Umbra.Core
    COMMAND UmbraCoreTests
)
