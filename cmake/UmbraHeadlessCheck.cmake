# Headless dependency gate (roadmap T53).
# Collects every library a target links, directly or through other targets,
# and adds a test that fails if any forbidden (window/GPU/editor) library appears.

function(umbra_collect_links target out_var)
    set(visited ${${out_var}})
    if(NOT TARGET ${target})
        list(APPEND visited ${target})
        set(${out_var} ${visited} PARENT_SCOPE)
        return()
    endif()
    get_target_property(aliased ${target} ALIASED_TARGET)
    if(aliased)
        set(target ${aliased})
    endif()
    if(target IN_LIST visited)
        return()
    endif()
    list(APPEND visited ${target})

    get_target_property(type ${target} TYPE)
    set(links)
    if(NOT type STREQUAL "INTERFACE_LIBRARY")
        get_target_property(direct ${target} LINK_LIBRARIES)
        if(direct)
            list(APPEND links ${direct})
        endif()
    endif()
    get_target_property(interface ${target} INTERFACE_LINK_LIBRARIES)
    if(interface)
        list(APPEND links ${interface})
    endif()

    foreach(link IN LISTS links)
        # $<LINK_ONLY:x> wraps private deps of static libraries; other generator expressions are skipped.
        string(REGEX REPLACE "^\\$<LINK_ONLY:(.*)>$" "\\1" link "${link}")
        if(NOT link MATCHES "^\\$<")
            umbra_collect_links(${link} visited)
        endif()
    endforeach()
    set(${out_var} ${visited} PARENT_SCOPE)
endfunction()

# umbra_add_headless_check(<test name> TARGETS <targets...> FORBIDDEN <regex>)
function(umbra_add_headless_check name)
    cmake_parse_arguments(ARG "" "FORBIDDEN" "TARGETS" ${ARGN})
    set(report "")
    foreach(target IN LISTS ARG_TARGETS)
        set(links)
        umbra_collect_links(${target} links)
        list(JOIN links ";" joined)
        string(APPEND report "${target}=${joined}\n")
    endforeach()
    set(file "${CMAKE_BINARY_DIR}/${name}.txt")
    file(WRITE "${file}" "${report}")
    add_test(
        NAME ${name}
        COMMAND ${CMAKE_COMMAND} -DREPORT=${file} "-DFORBIDDEN=${ARG_FORBIDDEN}"
                -P ${PROJECT_SOURCE_DIR}/cmake/UmbraHeadlessCheckRun.cmake
    )
endfunction()
