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

## 6. The analog screen (CRT and VHS)

An optional last pass makes the whole screen, menus and text included, look like an old television
playing a worn tape (S45, for The Last Curtain's analog horror; user 2026-11-23). `PostSettings::crt`
and `vhs` set each one's strength, 0 (off) to 1; both 0 means the pass doesn't run at all.

```
scene ──► composite + overlay ──► screen picture ──► analog pass ──► screen
                       (only when the analog screen is on; otherwise straight to the screen)
```

On Vulkan the frame and the overlay go into a **screen picture** (`VulkanPost`, one per swapchain
image) through a render pass built to be *compatible* with the swapchain's own (same attachments,
subpass and dependency; only the final layout differs), so the composite, ImGui and UI pipelines draw
into it unchanged. `AnalogPS` in `Post.hlsl` then draws it onto the swapchain. In the browser the
composite and UI passes target `UG.screen` and `umbra_gpu_analog` draws it onto the canvas
(`analogFs` in `umbra_render.js`; keep the two in step).

The pass, in order (`uv` 0..1, `t` seconds, wrapped at 1000 so the noise keeps its precision):

1. **Curved glass (CRT).** From the centre `c = 2·uv − 1`, a point looks further out:
   `c' = c · (1 + 0.08 · crt · |c|²)`. Corners land outside the picture and are drawn black, the tube's
   rim. `CrtWarp` in `Post.hpp` is the same formula, for tests.
2. **Shaking lines (VHS).** Each pair of pixel rows moves sideways by a random amount (up to 1.5
   pixels), changing 24 times a second. A **tracking band** about 7% of the screen tall rolls down
   once every 16 seconds and pulls its rows much further (up to 1.5% of the width). The bottom 3.5% of
   rows tear to the right (a tape's head switching).
3. **Colour fringe (CRT).** Red is read a little outward from the centre and blue a little inward, by
   `(uv − 0.5) · 0.006 · crt`: the colours drift apart towards the edges.
4. **Colour bleed (VHS).** Tape keeps brightness sharp but smears colour. The pixel's brightness
   (`luma = 0.299 r + 0.587 g + 0.114 b`) is kept; its colour (`rgb − luma`) is replaced by the average
   of 4 pixels to its left, mixed in by `0.7 · vhs`.
5. **Scanlines and stripes (CRT).** Brightness dips between lines 3 pixels apart:
   `scan = 0.5 + 0.5 cos(2π y / 3)`, `c *= 1 − 0.35 · crt · (1 − scan)`. Every pixel column lights one
   of the phosphors a little more (R, G, B in turn).
6. **Grain and wear (VHS).** Random grain (±3.5%), brighter noise inside the tracking band, a quarter
   of the colour washed out and the blacks lifted by 2%.
7. **Dark rim (CRT).** `c *= 1 − 0.5 · crt · max(|x|, |y|)⁶`, `x, y` from −1 to 1: the tube darkens
   only near its edges.

The random numbers are the usual `fract(sin(dot(p, (12.9898, 78.233))) · 43758.5453)` hash: cheap,
good enough for noise, no texture needed.

## 6b. A tracking tear (3D view only)

`PostSettings::tear` (0..1) draws a short VHS tracking fault into the finished 3D picture, in the
composite pass, so menus and text drawn after it stay whole (The Last Curtain's enemy turns). A band
`0.07` high sits at `y = 1 - tear`: as the game lowers `tear` back to 0 the band slides up the screen
and fades. Inside it, each row is pushed sideways by `tear * 0.04 * sin(y * 140)` and red and blue
are read `tear * 0.006` apart. It is packed into `PostBlock::sizes.w`. The browser build ignores it.

## 6c. Tilt-shift (3D view only)

A miniature look (HD-2D): a band across the screen stays sharp and the picture softens above and
below it. Set from code (`PostSettings::dof`, `dofCentre`, `dofWidth`); the scene file has no field.

For a pixel at height `y` (0 top, 1 bottom):

```
blur = dof * smoothstep(0, 0.25, |y - dofCentre| - dofWidth)
```

so inside the band (`dofWidth` either side of the centre) `blur` is 0, and it grows to `dof` over
the next quarter screen. The light there is the average of 17 samples: the pixel, 8 on a ring of
radius `blur` and 8 on a ring of half that. The radius is in screen heights, made round by scaling
x by height / width. `dof` is capped at 0.02. The web build does not blur (its composite has no
room for the extra numbers); everything else matches.

## 7. The `post` component

```json
"post": { "exposure": 0, "bloom": 0.5, "bloomThreshold": 1, "contrast": 1, "saturation": 1,
          "temperature": 0, "vignette": 0.25, "lut": "textures/lut_moonlight.png" }
```

There is one per scene; a second one gives the warning `scene.post.twice`. Without one, the defaults
above apply. A LUT image that is missing or not 256×16 is an error (`entity.post.lut`), reported with
the scene.

## 8. Checking without a GPU

`PostPixel` runs the same steps on one pixel on the CPU (`tests/core/PostTests.cpp`, T85): middle
gray is kept, highlights never pass white, a neutral grade changes nothing, each setting moves colors
the way its name says, and the vignette leaves the middle untouched.
