// WGSL counterpart of engine/render/shaders/Scene.hlsl for the W0 browser experiment.
// Same math and lighting; two differences forced by WebGPU (see docs/adr/0001-web-backend.md):
//  - WebGPU has no push constants: per-draw data is a uniform buffer bound with a dynamic offset.
//  - The base-color texture is left out of this sample (every draw uses its color only).

struct SceneUniform {
    view: mat4x4<f32>,
    projection: mat4x4<f32>,
};

struct DrawConstants {
    model: mat4x4<f32>,
    color: vec4<f32>,
};

@group(0) @binding(0) var<uniform> scene: SceneUniform;
@group(1) @binding(0) var<uniform> perDraw: DrawConstants;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) normal: vec3<f32>,
    @location(2) baseColor: vec4<f32>,
};

@vertex
fn VSMain(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    let worldPosition = perDraw.model * vec4<f32>(input.position, 1.0);
    output.position = scene.projection * scene.view * worldPosition;
    // Correct for rotation and uniform scale; non-uniform scale would need the inverse-transpose.
    let m = perDraw.model;
    output.normal = normalize(mat3x3<f32>(m[0].xyz, m[1].xyz, m[2].xyz) * input.normal);
    output.baseColor = perDraw.color;
    return output;
}

@fragment
fn PSMain(input: VertexOutput) -> @location(0) vec4<f32> {
    let lightDirection = normalize(vec3<f32>(0.4, 0.8, 0.5));
    let lighting = 0.25 + 0.75 * max(dot(normalize(input.normal), lightDirection), 0.0);
    return vec4<f32>(input.baseColor.rgb * lighting, input.baseColor.a);
}
