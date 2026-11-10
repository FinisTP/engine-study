find_package(glm CONFIG REQUIRED)

add_library(UmbraCore STATIC)
add_library(Umbra::Core ALIAS UmbraCore)

target_sources(
    UmbraCore
    PRIVATE
        src/Version.cpp
        src/FrameClock.cpp
        src/FixedStepAccumulator.cpp
        src/MeshData.cpp
        src/Lighting.cpp
        src/Post.cpp
        src/Skeleton.cpp
        src/Animator.cpp
        src/Particles.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Version.hpp
            include/FrameClock.hpp
            include/FixedStepAccumulator.hpp
            include/SlotMap.hpp
            include/DrawList.hpp
            include/Lighting.hpp
            include/Lighting.md
            include/Post.hpp
            include/Post.md
            include/Skeleton.hpp
            include/Animator.hpp
            include/Skeleton.md
            include/Particles.hpp
            include/Particles.md
            include/Handles.hpp
            include/Diagnostic.hpp
            include/MeshData.hpp
            include/RetireQueue.hpp
            include/Profiling.hpp
)

target_compile_features(UmbraCore PUBLIC cxx_std_20)

target_link_libraries(
    UmbraCore
    PUBLIC
        glm::glm
    PRIVATE
        Umbra::Warnings
        Umbra::Sanitizers
)
