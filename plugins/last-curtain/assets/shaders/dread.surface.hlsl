// The Last Curtain's floors after midnight (LastCurtain Art.md "The night's mood"): blood stains
// spread over the painted tiles. The tint's alpha says how far they have spread (0 = none, 1 = dawn).

float DreadHash(float2 p)
{
    return frac(sin(dot(p, float2(127.1F, 311.7F))) * 43758.5453F);
}

// Value noise: four corner hashes, blended smoothly.
float DreadNoise(float2 p)
{
    const float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0F - 2.0F * f);
    const float a = lerp(DreadHash(i), DreadHash(i + float2(1.0F, 0.0F)), f.x);
    const float b = lerp(DreadHash(i + float2(0.0F, 1.0F)), DreadHash(i + float2(1.0F, 1.0F)), f.x);
    return lerp(a, b, f.y);
}

void Surface(inout SurfaceData s, SurfaceInput i)
{
    // 1. The painting, tinted as a plain material would be.
    s.albedo = SampleBase(i.uv).rgb * i.tint.rgb;
    const float spread = i.tint.a;
    if (spread <= 0.01F)
        return;
    // 2. Two octaves of noise over the floor; more of it passes the threshold as the stains spread.
    const float2 p = i.worldPosition.xz * 0.9F;
    const float n = DreadNoise(p) * 0.65F + DreadNoise(p * 3.1F) * 0.35F;
    const float threshold = 1.0F - spread * 0.6F;
    const float stain = smoothstep(threshold, threshold + 0.03F, n);
    // 3. Fresh blood: dark red, glistening slowly.
    const float wet = 0.5F + 0.5F * sin(i.time * 1.3F + n * 9.0F);
    s.albedo = lerp(s.albedo, float3(0.26F, 0.015F, 0.02F) + wet * 0.04F, stain * 0.92F);
}
