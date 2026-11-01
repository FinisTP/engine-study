// set 0: per-frame camera data
struct SceneUniform
{
    column_major float4x4 view;
    column_major float4x4 projection;
};

// per-draw data (push constants, must match DrawConstants in VulkanVertexPipeline.hpp)
struct DrawConstants
{
    column_major float4x4 model;
    float4 color;
};

[[vk::binding(0, 0)]] ConstantBuffer<SceneUniform> scene;
[[vk::push_constant]] ConstantBuffer<DrawConstants> perDraw;

// set 1: per-texture descriptor
[[vk::combinedImageSampler]]
[[vk::binding(0, 1)]]
Texture2D<float4> baseColorTexture : register(t0, space1);

[[vk::combinedImageSampler]]
[[vk::binding(0, 1)]]
SamplerState baseColorSampler : register(s0, space1);

struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 normal : NORMAL;
    [[vk::location(1)]] float2 uv : TEXCOORD0;
    [[vk::location(2)]] float4 baseColor : COLOR0;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    const float4 worldPosition = mul(perDraw.model, float4(input.position, 1.0F));
    output.position = mul(scene.projection, mul(scene.view, worldPosition));
    // Correct for rotation and uniform scale; non-uniform scale would need the inverse-transpose.
    output.normal = normalize(mul((float3x3)perDraw.model, input.normal));
    output.uv = input.uv;
    output.baseColor = perDraw.color;
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    const float3 lightDirection = normalize(float3(0.4F, 0.8F, 0.5F));
    const float lighting = 0.25F + 0.75F * max(dot(normalize(input.normal), lightDirection), 0.0F);
    const float4 albedo = baseColorTexture.Sample(baseColorSampler, input.uv) * input.baseColor;
    return float4(albedo.rgb * lighting, albedo.a);
}
