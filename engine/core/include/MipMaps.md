# Mipmaps (`core::MipChain`)

A texture repeated across a big floor is sampled far away with many texels per screen pixel. With
only the full-size image, neighbouring pixels pick unrelated texels and the surface shimmers.
A **mip chain** stores the texture again at 1/2, 1/4, ... down to 1×1; the GPU picks the level
whose texel size matches the pixel (and blends two levels, `VK_SAMPLER_MIPMAP_MODE_LINEAR`).

```text
level 0   W x H            the texture
level 1   W/2 x H/2        each texel = average of a 2x2 block of level 0
...
level n   1 x 1            the average colour
```

Sizes are halved and rounded down (never below 1); at odd sizes the last row/column is reused
(clamped). Levels: `floor(log2(max(W, H))) + 1`, about 1/3 more memory in total.

Simplification: the average is taken on the stored sRGB values, not on linear light, so small
levels come out slightly darker on high-contrast textures. That is acceptable for this renderer;
a gamma-correct average would convert each value to linear, average, and convert back.

All levels are made on the CPU at load and uploaded together (`VulkanAllocator::CreateTexture`);
the sampler's `maxLod` lets the GPU use them.
