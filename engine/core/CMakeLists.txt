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
            include/umbra/core/Version.hpp
            include/umbra/core/FrameClock.hpp
            include/umbra/core/FixedStepAccumulator.hpp
)

target_compile_features(UmbraCore PUBLIC cxx_std_20)