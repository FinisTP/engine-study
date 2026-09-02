struct PushConstants
{
    float2 scale;
    float2 translate;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pushConstants;

[[vk::combinedImageSampler]]
[[vk::binding(0, 0)]]
Texture2D<float4> fontTexture : register(t0);

[[vk::combinedImageSampler]]
[[vk::binding(0, 0)]]
SamplerState fontSampler : register(s0);

struct VertexInput
{
    [[vk::location(0)]] float2 position : POSITION;
    [[vk::location(1)]] float2 uv : TEXCOORD0;
    [[vk::location(2)]] float4 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
    [[vk::location(1)]] float4 color : COLOR0;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position * pushConstants.scale + pushConstants.translate, 0.0F, 1.0F);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    return input.color * fontTexture.Sample(fontSampler, input.uv);
}
