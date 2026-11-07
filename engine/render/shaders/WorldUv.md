# World-space texture coordinates (`Scene.hlsl`)

Boxes from `scenes/phase2.gltf` have texture coordinates 0–1 on every face. Scaled to a 6 m wall,
one copy of the texture is stretched over all 6 m. A material with `"projection": "world"` repeats
the texture by **distance** instead (`metersPerRepeat`, sent per draw in `DrawConstants.texturing.x`).

In the vertex shader:

1. Take the world-space normal `n` and position `p`.
2. Pick the axis the surface faces most: the largest of `|n.x|`, `|n.y|`, `|n.z|`.
3. Use the other two world coordinates as the texture coordinates:

| Surface faces | uv |
|---|---|
| ±X (walls along Z) | `(p.z, -p.y)` |
| ±Y (floors, roofs) | `(p.x, p.z)` |
| ±Z (walls along X) | `(p.x, -p.y)` |

4. Divide by `metersPerRepeat`; the sampler's repeat mode wraps the result.

`-p.y` keeps "up" in the image pointing up on walls. Because the choice is made per vertex, it
suits flat faces (boxes, walls, floors, ramps). On curved models it would switch planes across a
triangle; those keep `"projection": "uv"` (the model's own coordinates). Textures stay fixed
in the world, so moving an object slides the texture across it: right for walls and terrain,
wrong for moving props.
