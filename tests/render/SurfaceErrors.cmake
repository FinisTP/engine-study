# T86: a broken surface shader is refused, and DXC names the surface file and its line.
# Run with -DDXC=... -DSHADERS=<engine/render/shaders> -DSOURCE=<broken file> -DWORK=<folder>.
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/broken.hlsl" "#include \"${SHADERS}/Surface.hlsli\"\n#define UMBRA_SURFACE 1\n#include \"${SOURCE}\"\n#include \"${SHADERS}/Scene.hlsl\"\n")
execute_process(COMMAND "${DXC}" -spirv -fspv-target-env=vulkan1.1 -T ps_6_0 -E PSMain -Fo "${WORK}/broken.spv" "${WORK}/broken.hlsl"
                RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(result EQUAL 0)
    message(FATAL_ERROR "the broken surface shader compiled")
endif()
if(NOT "${out}${err}" MATCHES "broken.surface.hlsl:5:")
    message(FATAL_ERROR "the error does not name broken.surface.hlsl line 5:\n${out}${err}")
endif()
message(STATUS "refused at broken.surface.hlsl:5 as expected")
