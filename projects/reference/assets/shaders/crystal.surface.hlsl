// Dungeon crystal: a slow glow pulse and bright facet edges that shimmer as you walk around it.
void Surface(inout SurfaceData s, SurfaceInput i)
{
    const float pulse = 0.75F + 0.25F * sin(i.time * 1.6F);
    const float3 toEye = normalize(scene.cameraPosition.xyz - i.worldPosition);
    const float edge = pow(1.0F - saturate(abs(dot(i.normal, toEye))), 3.0F); // brighter where it turns away
    const float shimmer = 0.5F + 0.5F * sin(i.worldPosition.y * 20.0F + i.time * 3.0F);
    s.emissive = s.emissive * pulse + edge * (2.0F + 2.0F * shimmer);
}
