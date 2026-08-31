find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraPlatformTests)

target_sources(
    UmbraPlatformTests
    PRIVATE
        ApplicationEventsTests.cpp
        InputTests.cpp
        PlatformLifetimeTests.cpp
        WindowTests.cpp
)

target_compile_features(UmbraPlatformTests PRIVATE cxx_std_20)

target_link_libraries(
    UmbraPlatformTests
    PRIVATE
        Umbra::Platform
        Catch2::Catch2WithMain
)

add_test(
    NAME Umbra.Platform
    COMMAND UmbraPlatformTests
)
