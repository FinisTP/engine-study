add_executable(Umbra)

target_sources(
    Umbra
    PRIVATE
        Main.cpp
)

target_compile_features(Umbra PRIVATE cxx_std_20)

target_link_libraries(
    Umbra
    PRIVATE
        Umbra::Core
        Umbra::Diagnostics
        Umbra::Platform
        Umbra::Warnings
        Umbra::Sanitizers
)

umbra_copy_asan_runtime(Umbra)
