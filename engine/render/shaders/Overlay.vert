#version 450
layout(push_constant) uniform PushConstants { vec2 scale; vec2 translate; } pushConstants;
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;
layout(location = 0) out vec2 fragmentUv;
layout(location = 1) out vec4 fragmentColor;
void main()
{
    gl_Position = vec4(inPosition * pushConstants.scale + pushConstants.translate, 0.0, 1.0);
    fragmentUv = inUv;
    fragmentColor = inColor;
}
