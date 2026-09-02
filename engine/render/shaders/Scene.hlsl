struct SceneUniform
{
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 projection;
    float4 baseColor;
};

[[vk::binding(0, 0)]] ConstantBuffer<SceneUniform> scene;

[[vk::combinedImageSampler]]
[[vk::binding(1, 0)]]
Texture2D<float4> baseColorTexture : register(t0);

[[vk::combinedImageSampler]]
[[vk::binding(1, 0)]]
SamplerState baseColorSampler : register(s0);

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
    const float4 worldPosition = mul(scene.model, float4(input.position, 1.0F));
    output.position = mul(scene.projection, mul(scene.view, worldPosition));
    output.normal = normalize(mul((float3x3)scene.model, input.normal));
    output.uv = input.uv;
    output.baseColor = scene.baseColor;
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    const float3 lightDirection = normalize(float3(0.4F, 0.8F, 0.5F));
    const float lighting = 0.25F + 0.75F * max(dot(normalize(input.normal), lightDirection), 0.0F);
    const float4 albedo = baseColorTexture.Sample(baseColorSampler, input.uv) * input.baseColor;
    return float4(albedo.rgb * lighting, albedo.a);
}
