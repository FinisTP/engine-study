#version 450
layout(binding = 0) uniform sampler2D fontTexture;
layout(location = 0) in vec2 fragmentUv;
layout(location = 1) in vec4 fragmentColor;
layout(location = 0) out vec4 outputColor;
void main() { outputColor = fragmentColor * texture(fontTexture, fragmentUv); }
