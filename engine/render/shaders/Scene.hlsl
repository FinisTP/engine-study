// The scene shaders (Vulkan). Lighting, fog, sky and shadows: engine/core/include/Lighting.md.
// The browser's copy is Scene.wgsl in web/umbra_render.js: keep the two in step. Custom surface
// shaders are this file's PSMain with UMBRA_WEB, converted to WGSL at build time.

#include "Surface.hlsli"

// A material without a custom shader keeps the plain surface. A custom one is compiled from a
// wrapper that includes its file first and defines UMBRA_SURFACE (CustomShaders.md).
#ifndef UMBRA_SURFACE
void Surface(inout SurfaceData s, SurfaceInput i)
{
}
#endif

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
    [[vk::location(3)]] float3 worldPosition : TEXCOORD1;
    [[vk::location(4)]] float emissive : TEXCOORD2;
};

#ifndef UMBRA_WEB
VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    const float4 worldPosition = mul(perDraw.model, float4(input.position, 1.0F));
    output.position = mul(scene.projection, mul(scene.view, worldPosition));
    // Correct for rotation and uniform scale; non-uniform scale would need the inverse-transpose.
    output.normal = normalize(mul((float3x3)perDraw.model, input.normal));
    output.uv = input.uv;
    if (perDraw.texturing.x > 0.0F)
    {
        // World-space texture coordinates (WorldUv.md): project onto the plane facing the normal,
        // so a wall 6 m long repeats the texture 6 / metersPerRepeat times instead of stretching it.
        const float3 n = abs(output.normal);
        const float3 p = worldPosition.xyz;
        const float2 planar = (n.x >= n.y && n.x >= n.z) ? float2(p.z, -p.y) : (n.y >= n.z) ? float2(p.x, p.z) : float2(p.x, -p.y);
        output.uv = planar / perDraw.texturing.x;
    }
    output.baseColor = perDraw.color;
    output.worldPosition = worldPosition.xyz;
    output.emissive = perDraw.texturing.y;
    return output;
}
#endif

// How much sun reaches this point: 1 = lit, 0 = in shadow. 3x3 samples soften the edge.
float SunShadow(float3 worldPosition, float3 normal)
{
    // 1. Into the shadow map, pushed a little off the surface so it doesn't shadow itself.
    const float4 s = mul(scene.shadowMatrix, float4(worldPosition + normal * 0.06F, 1.0F));
#ifdef UMBRA_WEB
    const float2 uv = float2(0.5F + 0.5F * s.x, 0.5F - 0.5F * s.y); // WebGPU: clip y = +1 is the first row
#else
    const float2 uv = s.xy * 0.5F + 0.5F; // Vulkan: clip y = -1 is the map's first row
#endif
    if (any(uv < 0.0F) || any(uv > 1.0F) || s.z > 1.0F)
        return 1.0F;
    // 2. Each sample answers "is this point nearer the sun than what the map saw?".
    const float texel = scene.counts.y;
    float lit = 0.0F;
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            lit += shadowMap.SampleCmpLevelZero(shadowSampler, uv + float2(x, y) * texel, s.z - 0.0015F);
    return lit / 9.0F;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    // 0. The surface: the plain material, then the material's own shader if it has one.
    SurfaceInput si;
    si.uv = input.uv;
    si.worldPosition = input.worldPosition;
    si.normal = normalize(input.normal);
    si.time = scene.cameraPosition.w;
    si.tint = input.baseColor;
    const float4 base = SampleBase(input.uv) * input.baseColor;
    SurfaceData surface;
    surface.albedo = base.rgb;
    surface.alpha = base.a;
    surface.normal = si.normal;
    surface.emissive = input.emissive;
    Surface(surface, si);
    const float3 n = normalize(surface.normal);
    const float3 p = input.worldPosition;

    // 1. Sky light from above, ground light from below.
    const float3 sky = lerp(scene.horizonColor.rgb, scene.skyColor.rgb, 0.5F);
    float3 light = lerp(scene.groundColor.rgb, sky, n.y * 0.5F + 0.5F) * scene.skyColor.w;

    // 2. The sun (or moon), unless something stands in the way.
    const float sunAmount = max(dot(n, scene.sunDirection.xyz), 0.0F);
    if (sunAmount > 0.0F)
        light += scene.sunColor.rgb * sunAmount * (scene.sunDirection.w > 0.0F ? SunShadow(p, n) : 1.0F);

    // 3. Point lights: softly to nothing at their range.
    const int count = (int)scene.counts.x;
    for (int i = 0; i < count; ++i)
    {
        const float3 toLight = scene.lightPositions[i].xyz - p;
        const float distance = length(toLight);
        const float x = distance / scene.lightPositions[i].w;
        if (x >= 1.0F)
            continue;
        const float window = saturate(1.0F - x * x * x * x);
        const float falloff = window * window / (1.0F + distance * distance);
        light += scene.lightColors[i].rgb * falloff * max(dot(n, toLight / max(distance, 0.0001F)), 0.0F);
    }

    // 4. Its own glow (flames, crystals), then fog: the farther, the more of the fog's color.
    float3 color = surface.albedo * (light + surface.emissive);
    const float fog = 1.0F - exp(-scene.fogColor.w * length(p - scene.cameraPosition.xyz));
    color = lerp(color, scene.fogColor.rgb, fog);
    return float4(color, surface.alpha);
}

// ---- Sun shadow map: depth only, seen from the sun ----

float4 ShadowVS(VertexInput input) : SV_Position
{
    return mul(scene.shadowMatrix, mul(perDraw.model, float4(input.position, 1.0F)));
}

// ---- Sky: one triangle covering the screen, drawn first ----

struct SkyOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 ndc : TEXCOORD0;
};

SkyOutput SkyVS(uint id : SV_VertexID)
{
    // Vertices (-1,-1), (3,-1), (-1,3): one triangle that covers the whole screen.
    SkyOutput output;
    const float2 ndc = float2((id << 1) & 2, id & 2) * 2.0F - 1.0F;
    output.position = float4(ndc, 1.0F, 1.0F);
    output.ndc = ndc;
    return output;
}

float4 SkyPS(SkyOutput input) : SV_Target0
{
    // 1. The view ray through this pixel.
    const float4 far = mul(scene.inverseViewProjection, float4(input.ndc, 1.0F, 1.0F));
    const float3 ray = normalize(far.xyz / far.w - scene.cameraPosition.xyz);
    // 2. Horizon to zenith above, horizon to ground below.
    float3 color = ray.y >= 0.0F ? lerp(scene.horizonColor.rgb, scene.skyColor.rgb, sqrt(ray.y))
                                 : lerp(scene.horizonColor.rgb, scene.groundColor.rgb, saturate(-ray.y * 4.0F));
    // 3. The sun: a bright disc and a soft glow around it.
    const float toward = max(dot(ray, scene.sunDirection.xyz), 0.0F);
    color += scene.sunColor.rgb * (pow(toward, 800.0F) * 6.0F + pow(toward, 12.0F) * 0.25F);
    // 4. Thick fog hides the sky too.
    color = lerp(color, scene.fogColor.rgb, saturate(scene.fogColor.w * 25.0F));
    return float4(color, 1.0F);
}
