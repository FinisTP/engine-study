# Tileable textures (`TextureTools.hpp`)

A wall or floor texture repeats many times. If its right edge doesn't continue into its left
edge (and bottom into top), every repeat shows a line: a **seam**. Image models rarely make
seamless pictures on their own, so Umbra fixes and measures this itself, for any generator.

## MakeSeamless: shift by half, blend the middle

One pass per direction. For left/right, with width `w`:

1. `S` = the image shifted by `w/2` with wrap-around: `S(x, y) = I((x + w/2) mod w, y)`.
   `S`'s left and right edges are columns from the **middle** of `I`, which sit next to each other,
   so `S` repeats without a jump. But `S` now has the old seam in its middle (`x = w/2`).
2. Blend: `out = S·(1 − k) + I·k`, where `k` is 1 on the centre column and falls linearly to 0
   over a band of `band · w/2` pixels on each side.

```text
k:  0 ............ 0 /‾‾1‾‾\ 0 ............ 0
    ^ edges: S (wraps)  ^ centre: I (no seam here)
```

At the edges the result is `S` (wraps cleanly); in the centre it is `I` (the old picture, which is
continuous there); in between both are mixed. Then the same along y. The cost is a soft
"double image" in the blend band, which on stone, grass or wood mostly reads as texture.

## SeamScore: is a seam visible?

```text
seam  = mean |I(w−1, y) − I(0, y)| over all y  +  mean |I(x, h−1) − I(x, 0)| over all x   (RGB average)
inner = mean difference between horizontal and vertical neighbours inside the image
score = seam / max(inner, 1)
```

A seamless texture's edges differ about as much as any two neighbouring pixels: score ≈ 1.
A score above `visibleSeam` (2) means the edges jump more than the texture normally does, and
the editor and `UmbraTool texture` warn (`texture.seam`). Measured on a left-to-right ramp, the
score drops from far above 2 to below 2 after `MakeSeamless` (`TextureToolsTests`).

## The rest

`Resize` is bilinear (enough to bring 1024 px pictures to the asked power of two). `DecodeImage`,
`EncodePng` and `ImageSize` use stb_image/stb_image_write, already built into the engine by tinygltf.
