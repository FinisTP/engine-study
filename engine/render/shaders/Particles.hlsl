// Particles (S9, engine/core/include/Particles.md): one camera-facing square per instance, drawn
// into the HDR scene after the solid things. The browser's copy is in web/umbra_render.js.

#include "Surface.hlsli"

struct ParticleInput
{
    [[vk::location(0)]] float4 positionSize : POSITION; // world position, size in meters
    [[vk::location(1)]] float4 color : COLOR0;
    [[vk::location(2)]] float4 shape : TEXCOORD0;       // ring (0 = soft dot), 1 if additive, angle, sheet
    [[vk::location(3)]] float4 stretchCell : TEXCOORD1; // world stretch vector, cell of the sheet
};

struct ParticleOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 corner : TEXCOORD0; // -1..1 across the square
    [[vk::location(1)]] float4 color : COLOR0;
    [[vk::location(2)]] float2 shape : TEXCOORD1;  // ring, additive
    [[vk::location(3)]] float3 picture : TEXCOORD2; // uv in the sheet, 1 if textured
};

// A point of the screen (x, y in -1..1) halfway into the depth range, in world space.
float3 Unproject(float2 ndc)
{
    const float4 p = mul(scene.inverseViewProjection, float4(ndc, 0.5F, 1.0F));
    return p.xyz / p.w;
}

ParticleOutput ParticleVS(ParticleInput input, uint id : SV_VertexID)
{
    // 1. Which corner of the square this vertex is (two triangles: 0 1 2, 2 1 3).
    const uint index = id < 3 ? id : 6 - id; // 0 1 2 3 2 1 -> corners 0..3
    const float2 corner = float2(index & 1, index >> 1) * 2.0F - 1.0F;
    // 2. The camera's right and up in the world: the square always faces the screen.
    const float3 right = normalize(Unproject(float2(1.0F, 0.0F)) - Unproject(float2(-1.0F, 0.0F)));
    const float3 up = normalize(Unproject(float2(0.0F, 1.0F)) - Unproject(float2(0.0F, -1.0F)));
    // 3. Turned by its angle; a stretched one lies along its stretch, longer by its length (Particles.md "Pictures").
    const float s = sin(input.shape.z), c = cos(input.shape.z);
    float3 across = right * c + up * s, along = up * c - right * s;
    float extra = 0.0F;
    const float3 facing = cross(right, up);
    const float3 stretch = input.stretchCell.xyz - dot(input.stretchCell.xyz, facing) * facing; // its part across the screen
    if (dot(stretch, stretch) > 1e-8F)
    {
        across = normalize(stretch);
        along = normalize(cross(facing, across));
        extra = sqrt(dot(input.stretchCell.xyz, input.stretchCell.xyz));
    }
    const float half = input.positionSize.w * 0.5F;
    const float3 world = input.positionSize.xyz + across * corner.x * (half + extra * 0.5F) + along * corner.y * half;
    // 4. Fog swallows far particles like everything else.
    const float fade = exp(-scene.fogColor.w * distance(world, scene.cameraPosition.xyz));
    // 5. The picture: cell = column + row * across, sheet = across + 16 * down.
    const float columns = max(fmod(input.shape.w, 16.0F), 1.0F), rows = max(floor(input.shape.w / 16.0F), 1.0F);
    const float cell = input.stretchCell.w;
    const float2 uv = (float2(fmod(cell, columns), floor(cell / columns)) + (corner * float2(1.0F, -1.0F) + 1.0F) * 0.5F) / float2(columns, rows);
    ParticleOutput output;
    output.position = mul(scene.projection, mul(scene.view, float4(world, 1.0F)));
    output.corner = corner;
    output.color = float4(input.color.rgb, input.color.a * fade);
    output.shape = input.shape.xy;
    output.picture = float3(uv, input.shape.w > 0.0F ? 1.0F : 0.0F);
    return output;
}

float4 ParticlePS(ParticleOutput input) : SV_Target0
{
    // A picture from the sheet; else a soft dot (bright middle, fading edge), or a ring with a hollow middle.
    const float d = length(input.corner);
    const float soft = saturate(1.0F - d);
    const float ring = saturate((1.0F - d) * 6.0F) * smoothstep(input.shape.x * 0.6F, input.shape.x, d);
    const float4 picture = SampleBase(input.picture.xy);
    float alpha = input.color.a * (input.shape.x > 0.0F ? ring : soft * soft);
    float3 rgb = input.color.rgb;
    if (input.picture.z > 0.5F)
        alpha = input.color.a * picture.a, rgb *= picture.rgb;
    // Premultiplied: alpha 0 adds light, alpha covers what is behind (one blend mode for both).
    return float4(rgb * alpha, input.shape.y > 0.5F ? 0.0F : alpha);
}
