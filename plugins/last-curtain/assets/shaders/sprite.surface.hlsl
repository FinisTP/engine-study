// The Last Curtain's guests as flat pictures (LastCurtain Motions.md "The shader"). The draw's colour is
// the tint colour (rgb) and the opacity (a); its emissive packs the burn and the tint amount:
// floor(emissive) = burn * 99, frac(emissive) = tint amount * 0.99.

float SpriteHash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898F, 78.233F))) * 43758.5453F);
}

void Surface(inout SurfaceData s, SurfaceInput i)
{
    // 1. Unpack the burn and the tint amount.
    const float burn = floor(s.emissive) / 99.0F;
    const float amount = frac(s.emissive) / 0.99F;
    const float4 picture = SampleBase(i.uv);
    // 2. Cut the picture out; a fading sprite drops a fixed scatter of its pixels; a burning one
    //    loses the pixels whose noise is below the burn.
    const float n = SpriteHash(floor(i.uv * 192.0F));
    clip(picture.a - 0.5F);
    clip(i.tint.a - n - 0.001F);
    clip(n - burn);
    // 3. The picture mixed toward the tint; the burning edge glows like an ember.
    const bool ember = burn > 0.0F && n < burn + 0.05F;
    s.albedo = ember ? float3(1.0F, 0.55F, 0.18F) : lerp(picture.rgb, i.tint.rgb, amount);
    s.alpha = 1.0F;
    s.emissive = ember ? 2.0F : 0.35F;
    // 4. Lit as a card that faces the camera.
    s.normal = normalize(scene.cameraPosition.xyz - i.worldPosition);
}
