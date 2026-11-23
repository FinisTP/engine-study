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

// ---------------------------------------------------------------------------------------------
// The analog screen (Post.md "6"): a last pass over the finished frame, menus and text included.
// post.exposure = (crt, vhs, seconds, 0); post.sizes = (1/width, 1/height, width, height).
// `source` is the finished frame (sRGB image: samples come back as linear light).

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898F, 78.233F))) * 43758.5453F);
}

// The CRT's bulge (Post.hpp CrtWarp): points move out by their squared distance from the centre.
float2 CrtWarp(float2 uv, float crt)
{
    const float2 c = uv * 2.0F - 1.0F;
    return c * (1.0F + 0.08F * crt * dot(c, c)) * 0.5F + 0.5F;
}

float4 AnalogPS(Output i) : SV_Target0
{
    const float crt = post.exposure.x, vhs = post.exposure.y, t = post.exposure.z;
    const float2 texel = post.sizes.xy, size = post.sizes.zw;
    // 1. CRT: the curved glass; outside it is the black of the tube.
    float2 uv = CrtWarp(i.uv, crt);
    if (any(uv < 0.0F) || any(uv > 1.0F))
        return float4(0.0F, 0.0F, 0.0F, 1.0F);
    // 2. VHS: each pair of lines shakes sideways a little; a tracking band rolls down the screen
    //    and pulls its lines further; the bottom lines tear to the right (head switching).
    const float row = floor(uv.y * size.y * 0.5F);
    float shift = (Hash(float2(row, floor(t * 24.0F))) - 0.5F) * 3.0F * texel.x * vhs;
    const float band = saturate(1.0F - abs(uv.y - frac(t * 0.06F)) / 0.035F);
    shift += band * (Hash(float2(row, t)) - 0.5F) * 0.03F * vhs;
    shift += saturate((uv.y - 0.965F) / 0.035F) * 0.02F * vhs;
    uv.x += shift;
    // 3. Colour: the CRT's fringe (red and blue drift apart towards the edges) ...
    const float2 fringe = (uv - 0.5F) * 0.006F * crt;
    float3 c = float3(source.Sample(sourceSampler, uv + fringe).r, source.Sample(sourceSampler, uv).g, source.Sample(sourceSampler, uv - fringe).b);
    // ... and the tape's bleed: brightness stays sharp, colour smears to the right.
    float3 smear = 0.0F;
    for (int k = 1; k <= 4; ++k)
        smear += source.Sample(sourceSampler, uv - float2(texel.x * 2.0F * k, 0.0F)).rgb * 0.25F;
    const float3 luma = float3(0.299F, 0.587F, 0.114F);
    c = lerp(c, dot(c, luma) + (smear - dot(smear, luma)), 0.7F * vhs);
    // 4. CRT: scanlines (darker between lines of 3 pixels) and the phosphor stripes (R, G, B columns).
    const float scan = 0.5F + 0.5F * cos(uv.y * size.y * 6.2831853F / 3.0F);
    c *= 1.0F - 0.35F * crt * (1.0F - scan);
    const int column = int(i.position.x) % 3;
    c *= 1.0F - 0.12F * crt + 0.24F * crt * float3(column == 0, column == 1, column == 2);
    // 5. VHS: grain everywhere, brighter noise in the tracking band, washed-out blacks and colour.
    c += (Hash(uv * size + t * 61.0F) - 0.5F) * 0.07F * vhs;
    c += band * Hash(float2(uv.x * size.x, row + t)) * 0.12F * vhs;
    c = lerp(c, dot(c, luma).xxx, 0.25F * vhs) * (1.0F - 0.06F * vhs) + 0.02F * vhs;
    // 6. CRT: the tube is darker towards its curved edges.
    const float2 edge = abs(i.uv - 0.5F) * 2.0F;
    c *= 1.0F - 0.5F * crt * pow(max(edge.x, edge.y), 6.0F);
    return float4(max(c, 0.0F), 1.0F);
}
