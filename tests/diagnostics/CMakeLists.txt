find_package(Catch2 3 CONFIG REQUIRED)

add_executable(UmbraDiagnosticsTests)
target_sources(UmbraDiagnosticsTests PRIVATE DiagnosticsFormattingTests.cpp)
target_compile_features(UmbraDiagnosticsTests PRIVATE cxx_std_20)
target_link_libraries(
    UmbraDiagnosticsTests
    PRIVATE
        Umbra::Diagnostics
        Umbra::Warnings
        Umbra::Sanitizers
        Catch2::Catch2WithMain
)

umbra_copy_asan_runtime(UmbraDiagnosticsTests)

add_test(NAME Umbra.Diagnostics.Formatting COMMAND UmbraDiagnosticsTests)

add_executable(UmbraDiagnosticsProbe)
target_sources(UmbraDiagnosticsProbe PRIVATE DiagnosticsProbe.cpp)
target_compile_features(UmbraDiagnosticsProbe PRIVATE cxx_std_20)
target_link_libraries(
    UmbraDiagnosticsProbe
    PRIVATE
        Umbra::Diagnostics
        Umbra::Warnings
        Umbra::Sanitizers
)

umbra_copy_asan_runtime(UmbraDiagnosticsProbe)

function(add_umbra_crash_probe scenario expected_text)
    add_test(
        NAME Umbra.Diagnostics.${scenario}
        COMMAND
            ${CMAKE_COMMAND}
            -DPROBE=$<TARGET_FILE:UmbraDiagnosticsProbe>
            -DSCENARIO=${scenario}
            -DEXPECTED_TEXT=${expected_text}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/RunCrashProbe.cmake
    )
endfunction()

add_umbra_crash_probe(fatal "Fatal diagnostics probe")
add_umbra_crash_probe(assert "Assertion diagnostics probe")
add_umbra_crash_probe(terminate "std::terminate called without an active exception")

if(WIN32)
    add_umbra_crash_probe(windows-exception "Unhandled Windows exception")
endif()
