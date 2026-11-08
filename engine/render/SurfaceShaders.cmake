# Custom surface shaders (S7, shaders/CustomShaders.md): every assets/shaders/<name>.surface.hlsl of
# the project and its plugins becomes a scene fragment shader. A wrapper includes Surface.hlsli,
# the surface file and Scene.hlsl, so DXC reports errors at the surface file's own lines.
#   vulkan: <shaders>/surface/<name>.frag.spv
#   web:    <build>/surface/<name>.wgsl (DXC with UMBRA_WEB, then naga converts SPIR-V to WGSL)
function(umbra_surface_shaders mode outVar)
    set(outputs)
    set(shaders "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    foreach(dir ${UMBRA_PLUGIN_ASSETS} ${UMBRA_ASSETS})
        file(GLOB found CONFIGURE_DEPENDS "${dir}/shaders/*.surface.hlsl")
        foreach(source ${found})
            get_filename_component(file "${source}" NAME)
            string(REPLACE ".surface.hlsl" "" name "${file}")
            set(wrapper "${CMAKE_CURRENT_BINARY_DIR}/surface-src/${name}.hlsl")
            file(WRITE "${wrapper}" "// Generated: ${file} inside the scene shader.\n#include \"${shaders}/Surface.hlsli\"\n#define UMBRA_SURFACE 1\n#include \"${source}\"\n#include \"${shaders}/Scene.hlsl\"\n")
            set(deps "${source}" "${shaders}/Scene.hlsl" "${shaders}/Surface.hlsli")
            if(mode STREQUAL "vulkan")
                set(output "${UMBRA_SHADER_OUTPUT_DIRECTORY}/surface/${name}.frag.spv")
                add_custom_command(
                    OUTPUT "${output}"
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${UMBRA_SHADER_OUTPUT_DIRECTORY}/surface"
                    COMMAND "${UMBRA_DXC_EXECUTABLE}" -spirv -fspv-target-env=vulkan1.1 -fspv-entrypoint-name=main -T ps_6_0 -E PSMain -Fo "${output}" "${wrapper}"
                    COMMAND "${UMBRA_SPIRV_VAL_EXECUTABLE}" --target-env vulkan1.1 "${output}"
                    DEPENDS ${deps}
                    COMMENT "Compiling surface shader ${file}"
                    VERBATIM)
            else()
                if(NOT UMBRA_DXC_EXECUTABLE OR NOT UMBRA_NAGA_EXECUTABLE)
                    message(FATAL_ERROR "${file}: the browser build needs dxc and naga (cargo install naga-cli) to convert custom shaders")
                endif()
                set(spirv "${CMAKE_CURRENT_BINARY_DIR}/surface-src/${name}.web.spv")
                set(output "${CMAKE_CURRENT_BINARY_DIR}/surface/${name}.wgsl")
                add_custom_command(
                    OUTPUT "${output}"
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/surface"
                    COMMAND "${UMBRA_DXC_EXECUTABLE}" -spirv -fspv-target-env=vulkan1.1 -fspv-entrypoint-name=main -T ps_6_0 -E PSMain -DUMBRA_WEB -Fo "${spirv}" "${wrapper}"
                    COMMAND "${UMBRA_NAGA_EXECUTABLE}" "${spirv}" "${output}"
                    DEPENDS ${deps}
                    COMMENT "Converting surface shader ${file} to WGSL"
                    VERBATIM)
            endif()
            list(APPEND outputs "${output}")
        endforeach()
    endforeach()
    set(${outVar} ${outputs} PARENT_SCOPE)
endfunction()
