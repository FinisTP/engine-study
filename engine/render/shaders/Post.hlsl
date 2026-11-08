// Post-processing (Vulkan): bloom down/up passes and the final composite. The math: engine/core/include/Post.md.
// The browser's copy is post WGSL in web/umbra_render.js: keep the two in step.

struct PostConstants
{
    float4 exposure; // x = 2^exposure, y = bloom strength, z = threshold, w = vignette
    float4 sizes;    // xy = one texel of the source image, z = LUT size
};
[[vk::push_constant]] ConstantBuffer<PostConstants> post;

[[vk::combinedImageSampler]] [[vk::binding(0, 0)]] Texture2D<float4> source : register(t0);
[[vk::combinedImageSampler]] [[vk::binding(0, 0)]] SamplerState sourceSampler : register(s0);
[[vk::combinedImageSampler]] [[vk::binding(1, 0)]] Texture2D<float4> bloom : register(t1);
[[vk::combinedImageSampler]] [[vk::binding(1, 0)]] SamplerState bloomSampler : register(s1);
[[vk::combinedImageSampler]] [[vk::binding(2, 0)]] Texture3D<float4> lut : register(t2);
[[vk::combinedImageSampler]] [[vk::binding(2, 0)]] SamplerState lutSampler : register(s2);

struct Output
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

// One triangle covering the screen; uv 0..1 with 0 at the top (Vulkan's first row).
Output FullscreenVS(uint id : SV_VertexID)
{
    Output o;
    const float2 uv = float2((id << 1) & 2, id & 2);
    o.position = float4(uv * 2.0F - 1.0F, 0.0F, 1.0F);
    o.uv = uv;
    return o;
}

// The average of 4 bilinear samples half a texel apart: a 4x4 texel box, smooth when halving.
float3 Box(float2 uv)
{
    const float2 d = post.sizes.xy * 0.5F;
    return (source.Sample(sourceSampler, uv + float2(-d.x, -d.y)).rgb + source.Sample(sourceSampler, uv + float2(d.x, -d.y)).rgb +
            source.Sample(sourceSampler, uv + float2(-d.x, d.y)).rgb + source.Sample(sourceSampler, uv + float2(d.x, d.y)).rgb) * 0.25F;
}

// First bloom level: only the light above the threshold, after exposure.
float4 BrightPS(Output i) : SV_Target0
{
    return float4(max(Box(i.uv) * post.exposure.x - post.exposure.z, 0.0F), 1.0F);
}

// Each smaller level: the box average of the one before.
float4 DownPS(Output i) : SV_Target0
{
    return float4(Box(i.uv), 1.0F);
}

// Going back up: a 3x3 tent of the smaller level, added onto the bigger one (blend ONE + ONE).
float4 UpPS(Output i) : SV_Target0
{
    const float2 d = post.sizes.xy;
    float3 c = source.Sample(sourceSampler, i.uv).rgb * 4.0F;
    c += (source.Sample(sourceSampler, i.uv + float2(d.x, 0)).rgb + source.Sample(sourceSampler, i.uv - float2(d.x, 0)).rgb +
          source.Sample(sourceSampler, i.uv + float2(0, d.y)).rgb + source.Sample(sourceSampler, i.uv - float2(0, d.y)).rgb) * 2.0F;
    c += source.Sample(sourceSampler, i.uv + d).rgb + source.Sample(sourceSampler, i.uv - d).rgb +
         source.Sample(sourceSampler, i.uv + float2(d.x, -d.y)).rgb + source.Sample(sourceSampler, i.uv + float2(-d.x, d.y)).rgb;
    return float4(c / 16.0F, 1.0F);
}

float3 Tonemap(float3 x)
{
    x = max(x, 0.0F) * 0.8F; // ACES fit (Narkowicz); 0.8 keeps middle gray in place
    return saturate(x * (2.51F * x + 0.03F) / (x * (2.43F * x + 0.59F) + 0.14F));
}

float3 ToSrgb(float3 c)
{
    c = saturate(c);
    return select(c <= 0.0031308F, c * 12.92F, 1.055F * pow(c, 1.0F / 2.4F) - 0.055F);
}

float3 ToLinear(float3 c)
{
    return select(c <= 0.04045F, c / 12.92F, pow((c + 0.055F) / 1.055F, 2.4F));
}

// The final picture: exposure + bloom, tonemap, color table, vignette. Writes linear light
// (the sRGB screen encodes it).
float4 CompositePS(Output i) : SV_Target0
{
    // 1. HDR light plus the bloom's glow.
    const float3 hdr = source.Sample(sourceSampler, i.uv).rgb * post.exposure.x + bloom.Sample(bloomSampler, i.uv).rgb * post.exposure.y;
    // 2. To 0..1, then the grade: the LUT is indexed by sRGB values, centered on its cells.
    const float n = post.sizes.z;
    const float3 cell = ToSrgb(Tonemap(hdr)) * ((n - 1.0F) / n) + 0.5F / n;
    float3 color = ToLinear(lut.SampleLevel(lutSampler, cell, 0).rgb);
    // 3. Darker towards the corners.
    const float d = length(i.uv - 0.5F) * 1.41421356F;
    color *= 1.0F - post.exposure.w * d * d;
    return float4(color, 1.0F);
}
