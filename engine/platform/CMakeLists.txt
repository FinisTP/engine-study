find_package(SDL3 CONFIG REQUIRED)

add_library(UmbraPlatform STATIC)
add_library(Umbra::Platform ALIAS UmbraPlatform)

target_sources(
    UmbraPlatform
    PRIVATE
        src/Platform.cpp
        src/Input.cpp
        src/EventPump.cpp
        src/ApplicationEvents.cpp
        src/Window.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/Platform.hpp
            include/Input.hpp
            include/EventPump.hpp
            include/ApplicationEvents.hpp
            include/Window.hpp
)

target_compile_features(UmbraPlatform PUBLIC cxx_std_20)

target_link_libraries(
    UmbraPlatform
    PRIVATE
        Umbra::Diagnostics
        SDL3::SDL3
        Umbra::Warnings
        Umbra::Sanitizers
)
