if(NOT DEFINED PROBE OR NOT DEFINED SCENARIO OR NOT DEFINED EXPECTED_TEXT)
    message(FATAL_ERROR "PROBE, SCENARIO, and EXPECTED_TEXT are required")
endif()

execute_process(
    COMMAND "${PROBE}" "${SCENARIO}"
    TIMEOUT 10
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_stdout
    ERROR_VARIABLE probe_stderr
)

if("${probe_result}" STREQUAL "0")
    message(FATAL_ERROR "Diagnostics probe '${SCENARIO}' unexpectedly exited successfully")
endif()

if("${probe_result}" STREQUAL "77")
    message(STATUS "Diagnostics probe '${SCENARIO}' is disabled in this configuration")
    return()
endif()

set(probe_output "${probe_stdout}\n${probe_stderr}")
string(FIND "${probe_output}" "${EXPECTED_TEXT}" expected_position)

if(expected_position EQUAL -1)
    message(FATAL_ERROR
        "Diagnostics probe '${SCENARIO}' did not emit '${EXPECTED_TEXT}'.\n"
        "Result: ${probe_result}\n"
        "Output:\n${probe_output}"
    )
endif()
