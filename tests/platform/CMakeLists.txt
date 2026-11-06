find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraPlatformTests)

target_sources(
    UmbraPlatformTests
    PRIVATE
        ApplicationEventsTests.cpp
        AudioDeviceTests.cpp
        InputTests.cpp
        PlatformLifetimeTests.cpp
        WindowTests.cpp
)

target_compile_features(UmbraPlatformTests PRIVATE cxx_std_20)

target_link_libraries(
    UmbraPlatformTests
    PRIVATE
        Umbra::Platform
        Umbra::Audio
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraPlatformTests)

add_test(
    NAME Umbra.Platform
    COMMAND UmbraPlatformTests
)

# The audio test must not need a sound card.
set_tests_properties(Umbra.Platform PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=dummy")
