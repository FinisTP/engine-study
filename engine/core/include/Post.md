# Post-processing (S7)

The lit scene (`Lighting.md`) is drawn into an **HDR** picture: a 16-bit float per channel, so
light can be brighter than 1 (a torch flame may be 4). Post-processing turns that picture into
what the screen can show. It is set per scene by the `post` component, and `Post.hpp` holds the
numbers both renderers use:

```
HDR scene ──► bloom down/up ──► composite: exposure + bloom → tonemap → color table → vignette ──► screen
```

The Vulkan passes are in `engine/render/shaders/Post.hlsl` and `VulkanPost.cpp`. The browser's copy
is `postShader` in `engine/render/web/umbra_render.js`.

## 1. Exposure

```
light = hdr * 2^exposure
```

Exposure is counted in *stops*, like a camera: +1 doubles the light and -1 halves it.

## 2. Bloom

Very bright light bleeds into its surroundings, in the eye and in a camera lens. Bloom fakes this:

1. **Bright pass.** Keep only the light above the threshold: `max(light - threshold, 0)`.
   The result goes into level 0, which is half the screen size.
2. **Down.** Each level is half the size of the one before. It averages 4 bilinear samples taken
   half a texel apart, which covers a 4×4 texel box. Halving six times spreads the light far
   without a huge blur kernel.
3. **Up.** Starting from the smallest level, each level is read with a 3×3 *tent* filter
   (weights 1-2-1 × 1-2-1, divided by 16) and **added** onto the next bigger level (blend ONE + ONE).
   Level 0 then holds every blur size summed together: a sharp halo near the flame and a wide glow
   further out.
4. The composite adds `level0 * bloom` to the light.

| Quality | First level | Levels | Used by |
|---|---|---|---|
| Full | 1/2 of the screen | 6 | PC, browser |
| Reduced | 1/4 of the screen | 4 | Android (user decision 2026-11-09) |

## 3. Tonemapping

A screen shows 0..1, but the HDR light goes far above 1. Clipping at 1 turns every flame into a flat
white blob. A *tonemap* curve rolls highlights off gently instead. Umbra uses the ACES film curve,
in Narkowicz's fit:

```
x = light * 0.8
tonemap(x) = saturate( x(2.51x + 0.03) / (x(2.43x + 0.59) + 0.14) )
```

The 0.8 keeps middle gray (0.18) at about 0.2, so a scene looks as bright as it did before S7.
Above 1 the curve flattens slowly towards white and never passes it.

## 4. Color grade (the color table)

A *LUT* (look-up table) is a 16×16×16 cube. Each cell answers: "this color (in sRGB) should become
that color". The GPU blends the 8 cells around the color (trilinear filtering), so 16 steps per
channel are enough for smooth results.

- **Image layout.** A LUT image is 256×16: 16 slices of 16×16. The cell (r, g, b) is the pixel at
  `x = b·16 + r, y = g`. This is the common layout that paint programs and other engines use.
- **Baking.** `BakeGradeLut` builds one table from all of the grade at once, so the GPU does a
  single lookup however many settings there are:
  1. The cell's color, sent through the LUT image if the scene names one.
  2. White balance (`temperature`): red × (1 + 0.12t), green × (1 + 0.02t), blue × (1 − 0.15t), in linear light.
  3. Saturation: mix between the color and its gray (luma = 0.2126 R + 0.7152 G + 0.0722 B).
  4. Contrast around 0.5, in sRGB values (there it looks even across darks and lights).
- **Lookup.** The tonemapped color is turned into sRGB, scaled to land on cell centers
  (`c·15/16 + 0.5/16`), looked up, and turned back to linear light. The table is stored as plain
  UNORM, and the shader does the sRGB conversions itself.
- The table is rebuilt only when the grade changes, normally on a scene change. On Vulkan this
  waits for the GPU once.

`tools/textures/make_lut.py` writes the example `textures/lut_moonlight.png`, and an artist's own
LUT image can replace it.

## 5. Vignette

```
d = distance(uv, center) · √2      (0 in the middle, 1 in a corner)
color *= 1 − vignette · d²
```

## 6. The `post` component

```json
"post": { "exposure": 0, "bloom": 0.5, "bloomThreshold": 1, "contrast": 1, "saturation": 1,
          "temperature": 0, "vignette": 0.25, "lut": "textures/lut_moonlight.png" }
```

There is one per scene; a second one gives the warning `scene.post.twice`. Without one, the defaults
above apply. A LUT image that is missing or not 256×16 is an error (`entity.post.lut`), reported with
the scene.

## 7. Checking without a GPU

`PostPixel` runs the same steps on one pixel on the CPU (`tests/core/PostTests.cpp`, T85): middle
gray is kept, highlights never pass white, a neutral grade changes nothing, each setting moves colors
the way its name says, and the vignette leaves the middle untouched.
