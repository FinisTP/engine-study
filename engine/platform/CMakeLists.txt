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
            include/umbra/platform/Platform.hpp
            include/umbra/platform/Input.hpp
            include/umbra/platform/EventPump.hpp
            include/umbra/platform/ApplicationEvents.hpp
            include/umbra/platform/Window.hpp
)

target_compile_features(UmbraPlatform PUBLIC cxx_std_20)

target_link_libraries(
    UmbraPlatform
    PRIVATE
        Umbra::Diagnostics
        SDL3::SDL3
)