find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraCoreTests)

target_sources(
    UmbraCoreTests
    PRIVATE
        VersionTests.cpp
        FrameClockTests.cpp
)

target_compile_features(UmbraCoreTests PRIVATE cxx_std_20)

target_link_libraries(
    UmbraCoreTests
    PRIVATE
        Umbra::Core
        Catch2::Catch2WithMain
)

add_test(
    NAME Umbra.Core
    COMMAND UmbraCoreTests
)