// Well water: two scrolling ripple layers bend the normal and the texture lookup.
void Surface(inout SurfaceData s, SurfaceInput i)
{
    const float2 p = i.worldPosition.xz;
    const float a = sin(p.x * 3.0F + i.time * 1.7F) + sin(p.y * 4.0F - i.time * 1.3F);
    const float b = cos(p.x * 5.0F - i.time * 2.1F) + cos(p.y * 3.5F + i.time * 1.1F);
    s.normal = normalize(i.normal + float3(a, 0.0F, b) * 0.08F);
    s.albedo = SampleBase(i.uv + float2(a, b) * 0.01F).rgb * i.tint.rgb;
    s.emissive += 0.05F; // a little light of its own, so it reads in shade
}
