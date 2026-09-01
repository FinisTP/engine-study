#version 450
layout(binding = 0) uniform SceneUniform { mat4 model; mat4 view; mat4 projection; vec4 baseColor; } scene;
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 0) out vec3 fragmentNormal;
layout(location = 1) out vec2 fragmentUv;
layout(location = 2) out vec4 fragmentBaseColor;
void main()
{
    gl_Position = scene.projection * scene.view * scene.model * vec4(inPosition, 1.0);
    fragmentNormal = normalize(mat3(scene.model) * inNormal);
    fragmentUv = inUv;
    fragmentBaseColor = scene.baseColor;
}
