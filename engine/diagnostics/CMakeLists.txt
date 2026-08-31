find_package(spdlog CONFIG REQUIRED)

add_library(UmbraDiagnostics STATIC)
add_library(Umbra::Diagnostics ALIAS UmbraDiagnostics)

target_sources(
    UmbraDiagnostics
    PRIVATE
        src/Log.cpp
        src/CrashHandler.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/umbra/diagnostics/Log.hpp
            include/umbra/diagnostics/CrashHandler.hpp
)

target_compile_features(UmbraDiagnostics PUBLIC cxx_std_20)

target_link_libraries(
    UmbraDiagnostics
    PRIVATE
        spdlog::spdlog
)