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
            include/Log.hpp
            include/CrashHandler.hpp
)

target_compile_features(UmbraDiagnostics PUBLIC cxx_std_20)

target_link_libraries(
    UmbraDiagnostics
    PRIVATE
        spdlog::spdlog
        Umbra::Warnings
        Umbra::Sanitizers
)

# Windows: MiniDumpWriteDump for crash dumps (CrashHandler.cpp).
if(WIN32)
    target_link_libraries(UmbraDiagnostics PRIVATE dbghelp)
endif()

# Android: spdlog's logcat sink needs the system log library.
if(ANDROID)
    target_link_libraries(UmbraDiagnostics PRIVATE log)
endif()
