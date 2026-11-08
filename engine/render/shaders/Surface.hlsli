// What a scene shader and a custom surface shader share (S7, CustomShaders.md): the scene's data,
// the base texture, and the two structs a surface function sees.
// UMBRA_WEB picks the browser's bindings (the result is converted to WGSL at build time).
#ifndef UMBRA_SURFACE_HLSLI
#define UMBRA_SURFACE_HLSLI

// Per-frame data (Vulkan: SceneUniform in VulkanFrameResources.hpp; browser: Frame in RendererWeb.cpp).
struct SceneUniform
{
#ifdef UMBRA_WEB
    column_major float4x4 viewProjection;
#else
    column_major float4x4 view;
    column_major float4x4 projection;
#endif
    column_major float4x4 shadowMatrix;          // world -> sun shadow map (depth 0..1)
    column_major float4x4 inverseViewProjection; // screen -> world (the sky's view rays)
    float4 cameraPosition; // w = seconds
    float4 sunDirection;   // towards the sun; w = 1 if it casts shadows
    float4 sunColor;
    float4 skyColor;       // straight up; w = ambient strength
    float4 horizonColor;
    float4 groundColor;
    float4 fogColor;       // w = density per meter
    float4 counts;         // x = point lights, y = shadow texel size
    float4 lightPositions[16]; // xyz, w = range
    float4 lightColors[16];
};

// Per-draw data (Vulkan: push constants, DrawConstants in VulkanVertexPipeline.hpp; browser: a uniform slot).
struct DrawConstants
{
    column_major float4x4 model;
    float4 color;
    float4 texturing; // x = meters per repeat for world-space texture coordinates (0 = mesh UVs), y = emissive
};

#ifdef UMBRA_WEB
[[vk::binding(0, 0)]] ConstantBuffer<SceneUniform> scene;
[[vk::binding(1, 0)]] ConstantBuffer<DrawConstants> perDraw;
[[vk::binding(2, 0)]] Texture2D<float> shadowMap;
[[vk::binding(3, 0)]] SamplerComparisonState shadowSampler;
[[vk::binding(0, 1)]] SamplerState baseColorSampler;
[[vk::binding(1, 1)]] Texture2D<float4> baseColorTexture;
#else
[[vk::binding(0, 0)]] ConstantBuffer<SceneUniform> scene;
[[vk::push_constant]] ConstantBuffer<DrawConstants> perDraw;
[[vk::combinedImageSampler]] [[vk::binding(1, 0)]] Texture2D<float> shadowMap : register(t1, space0);
[[vk::combinedImageSampler]] [[vk::binding(1, 0)]] SamplerComparisonState shadowSampler : register(s1, space0);
[[vk::combinedImageSampler]] [[vk::binding(0, 1)]] Texture2D<float4> baseColorTexture : register(t0, space1);
[[vk::combinedImageSampler]] [[vk::binding(0, 1)]] SamplerState baseColorSampler : register(s0, space1);
#endif

// What a surface function gets...
struct SurfaceInput
{
    float2 uv;            // texture coordinates (world-space ones if the material repeats by meters)
    float3 worldPosition; // meters
    float3 normal;        // unit length, world space
    float time;           // seconds since the game started
    float4 tint;          // the entity's color times the mesh color
};

// ...and what it may change. Starts as the plain material: texture * tint, the mesh normal, the entity's emissive.
struct SurfaceData
{
    float3 albedo;
    float alpha;
    float3 normal;  // keep it unit length
    float emissive; // glow added to the light (Lighting.md)
};

// The material's texture at any coordinates (for scrolling, distortion...).
float4 SampleBase(float2 uv)
{
    return baseColorTexture.Sample(baseColorSampler, uv);
}

#endif
