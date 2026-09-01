option(UMBRA_ENABLE_ASAN "Enable AddressSanitizer" OFF)

add_library(UmbraSanitizers INTERFACE)
add_library(Umbra::Sanitizers ALIAS UmbraSanitizers)

if(UMBRA_ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(UmbraSanitizers INTERFACE /fsanitize=address)
        target_compile_definitions(
            UmbraSanitizers
            INTERFACE
                _DISABLE_VECTOR_ANNOTATION
                _DISABLE_STRING_ANNOTATION
                _DISABLE_OPTIONAL_ANNOTATION
        )
        target_link_options(UmbraSanitizers INTERFACE /INCREMENTAL:NO)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(UmbraSanitizers INTERFACE -fsanitize=address)
        target_link_options(UmbraSanitizers INTERFACE -fsanitize=address)
    else()
        message(FATAL_ERROR "UMBRA_ENABLE_ASAN is unsupported by ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

function(umbra_copy_asan_runtime target)
    if(NOT UMBRA_ENABLE_ASAN OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        return()
    endif()

    get_filename_component(msvc_compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(asan_runtime "${msvc_compiler_directory}/clang_rt.asan_dynamic-x86_64.dll")

    if(NOT EXISTS "${asan_runtime}")
        message(FATAL_ERROR "MSVC AddressSanitizer runtime was not found at ${asan_runtime}")
    endif()

    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${asan_runtime}"
            "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Copying the MSVC AddressSanitizer runtime for ${target}"
        VERBATIM
    )
endfunction()

function(umbra_copy_asan_runtime target)
    if(NOT UMBRA_ENABLE_ASAN OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        return()
    endif()

    get_filename_component(msvc_compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(asan_runtime "${msvc_compiler_directory}/clang_rt.asan_dynamic-x86_64.dll")

    if(NOT EXISTS "${asan_runtime}")
        message(FATAL_ERROR "MSVC AddressSanitizer runtime was not found at ${asan_runtime}")
    endif()

    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            "${asan_runtime}"
            "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Copying the MSVC AddressSanitizer runtime for ${target}"
        VERBATIM
    )
endfunction()
