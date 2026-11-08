// A surface shader with a mistake on line 5 (T86): the build must refuse it and name this line.
void Surface(inout SurfaceData s, SurfaceInput i)
{
    s.albedo = float3(1.0F, 0.5F, 0.2F);
    s.emissive = undefinedThing * 2.0F;
}
