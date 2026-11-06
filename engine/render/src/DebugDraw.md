# Debug lines

`core::DrawList::lines` holds world-space line segments (collider outlines, rays, the player capsule). They're drawn by the ImGui overlay on top of the scene, with no depth test. That way debug drawing needs no extra Vulkan pipeline.

## From world to pixels

```text
clip  = projection * view * (p, 1)        same matrices as the scene shader
ndc   = clip.xy / clip.w                  -1..1 on both axes
pixel = (ndc * 0.5 + 0.5) * screenSize    ImGui's origin is the top-left corner
```

The scene's projection has its Y axis flipped for Vulkan (`projection[1][1] *= -1`), so `ndc.y = -1` is the top of the screen. That already matches ImGui's downward Y, so no extra flip is needed.

## Points behind the camera

A point behind the camera has `clip.w <= 0`, and dividing by it mirrors the point across the screen. Before the divide, a segment is clipped against a tiny positive `w`:

```text
both ends behind        -> skip the line
one end behind (a)      -> a = a + (b - a) * (nearW - a.w) / (b.w - a.w)
```

That moves `a` along the segment to the point where `w = nearW`, which is the visible part's start. Clipping in clip space (before the divide) is exact because clip coordinates are linear along the segment.
