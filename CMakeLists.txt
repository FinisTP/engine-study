cmake_minimum_required(VERSION 3.28)

project(
    Umbra
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(engine/core)
add_subdirectory(engine/diagnostics)
add_subdirectory(engine/platform)
add_subdirectory(apps/umbra)

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests/core)
    add_subdirectory(tests/platform)
endif()
