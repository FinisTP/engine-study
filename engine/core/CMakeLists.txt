add_library(UmbraCore STATIC)
add_library(Umbra::Core ALIAS UmbraCore)

target_sources(
    UmbraCore
    PRIVATE
        src/Version.cpp
        src/FrameClock.cpp
        src/FixedStepAccumulator.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Version.hpp
            include/FrameClock.hpp
            include/FixedStepAccumulator.hpp
)

target_compile_features(UmbraCore PUBLIC cxx_std_20)

target_link_libraries(
    UmbraCore
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)

target_link_libraries(
    UmbraCore
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
