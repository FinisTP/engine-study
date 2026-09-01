#version 450
layout(binding = 1) uniform sampler2D baseColorTexture;
layout(location = 0) in vec3 fragmentNormal;
layout(location = 1) in vec2 fragmentUv;
layout(location = 2) in vec4 fragmentBaseColor;
layout(location = 0) out vec4 outputColor;
void main()
{
    vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.5));
    float lighting = 0.25 + 0.75 * max(dot(normalize(fragmentNormal), lightDirection), 0.0);
    vec4 albedo = texture(baseColorTexture, fragmentUv) * fragmentBaseColor;
    outputColor = vec4(albedo.rgb * lighting, albedo.a);
}
