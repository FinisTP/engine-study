# Lighting, sky, fog and shadows (S6)

`Lighting.hpp` turns a scene's `environment` component and its `light` components into the numbers
both renderers use: `Scene.hlsl` (Vulkan) and the WGSL copy in `umbra_render.js` (browser).
This file explains the math.

## 1. Linear light

Light adds up correctly only in *linear* values, where 0.5 really means half as much light.
Image files store *sRGB* values, which spend more numbers on dark shades.

- Textures are uploaded as sRGB formats (`R8G8B8A8_SRGB`, `rgba8unorm-srgb`). The GPU converts
  them to linear when the shader samples them.
- The screen is an sRGB target (`B8G8R8A8_SRGB`, or the canvas viewed as `bgra8unorm-srgb`).
  The GPU converts the linear result back when it writes the pixel.
- So the shader works only in linear values and never calls `pow(x, 2.2)` itself.
- Since S7 the scene is drawn into an HDR (16-bit float) picture first. Exposure, bloom, tonemapping
  and the color grade then turn it into the screen's 0..1 (`Post.md`).

## 2. What lights a pixel

```
color = albedo * (ambient + sun + point lights + emissive)
color = mix(color, fogColor, fog)
```

### Ambient (sky and ground)

Light from the sky arrives from above, and light bounced off the ground arrives from below.
The normal's `y` chooses between the two colors:

```
t       = n.y * 0.5 + 0.5           # 0 facing down, 1 facing up
ambient = mix(ground, (horizon + zenith) / 2, t) * ambientStrength
```

### Sun or moon

This is Lambert's law: a surface gets `cos(angle)` of the light, which is `dot(n, L)` for unit
vectors. The result is multiplied by the shadow term (section 4).

```
sun = sunColor * max(dot(n, sunDirection), 0) * shadow
```

### Point lights (torches, crystals)

Real light falls off as `1 / d²`, but that never reaches zero, so every light would touch every
pixel. A *window* brings it smoothly to zero at the light's `range`:

```
x       = d / range                  # 0 at the light, 1 at its edge
window  = saturate(1 - x^4)^2        # 1 near the light, 0 at the edge, smooth
falloff = window / (1 + d²)          # the +1 avoids infinity at d = 0
light  += color * intensity * falloff * max(dot(n, toLight), 0)
```

Up to 16 lights are drawn (`maxLights`). `PackLighting` keeps the 16 with the smallest
`distance to camera - range`, so a big light far away still beats a small one nearby.

Flicker scales the intensity by 0.5..1 using two sine waves at unrelated speeds, shifted per light,
so neighboring torches don't pulse together.

### Emissive

`emissive` is added straight to the light, so the surface shows its own color even in the dark.
A value of 1 shows the texture as it is, and 2–3 makes it glow. It doesn't light anything around
it: put a `light` next to it for that.

### Fog

This is exponential fog. Each meter of air lets through the same fraction of light:

```
fog     = 1 - exp(-density * distance)   # 0 near, towards 1 far away
density = 0.04 * fogSetting²             # setting 0..1; squared so small values stay subtle
```

At setting 0.3, density is 0.0036: a point 100 m away is 30% fog. At setting 1, it is 98% fog.

## 3. Time of day

`hour` runs 0..24. `dayLength` is the number of real seconds in one game day (720 = 12 minutes;
0 stops the clock).

```
hour += seconds * 24 / dayLength   (wrapped to 0..24)
```

The sun's position is an arc that rises at 6:00, is highest at 12:00 (65°) and sets at 18:00:

```
elevation = sin((h - 6) / 12 * π) * 65°
azimuth   = sunHeading + (h - 12) / 12 * 180°
direction = (cos(el) sin(az), sin(el), cos(el) cos(az))      # towards the sun
```

At night the elevation is negative. The *moon* is then used: it sits opposite the sun (`-direction`)
and its color is a dim blue, which keeps the night readable. Below 0.05 the direction's `y` is
raised so light never comes from below the horizon.

The colors come from a table of keys (`keys[]` in `Lighting.cpp`): dawn 6:00, morning,
noon, afternoon, dusk 18:00, and night. Each key holds the sun or moon color, the sky colors at the
zenith and horizon, the ground color and the ambient strength. The two keys around the hour
are mixed linearly. `tint` multiplies everything, for example for a green swamp.

**Indoors** (`indoor: true`) there is no sun, no sky and no shadows, only a faint tinted ambient
light, so torches do the work.

## 4. Sun shadows (shadow map)

1. **Draw the scene from the sun.** The camera is orthographic, because sunlight is parallel. It
   draws only depth into a 2048×2048 map: for each texel, how far the first surface is from the sun.
2. **Draw the scene from the camera.** Each pixel is moved into the sun's view with the same matrix.
   If the pixel is farther from the sun than the map's depth, something stands in the way, so the
   pixel is in shadow.

Details:

- **The box.** `SunShadowMatrix` covers a square of 25 m radius around the camera's target. Its
  depth reaches 3 radii towards and away from the sun, so tall things outside the square still cast.
- **Texel snapping.** The box's center is rounded to whole texels in the sun's view. Otherwise the
  edges of shadows crawl when the camera moves.
- **Bias.** A surface would shadow itself because of the map's limited precision ("shadow acne").
  Three things prevent it: the shadow pass uses a depth bias (constant and slope), the lookup point
  is moved 6 cm along the normal, and 0.0015 is subtracted from the compared depth.
- **Soft edge (PCF).** 3×3 neighboring texels are compared, and the results (each 0 or 1) are
  averaged. The comparison sampler (`SampleCmp`, `textureSampleCompareLevel`) does each compare in
  hardware, with linear filtering when available.
- **Y direction.** Vulkan's clip y = -1 is the map's first row, so `uv.y = 0.5 + 0.5*y`.
  WebGPU's clip y = +1 is the first row, so `uv.y = 0.5 - 0.5*y`. The matrix is the same for both.

## 5. Sky

A single triangle covers the screen, drawn first with no depth test. For each pixel, the inverse
view-projection matrix turns the screen position into a world point on the far plane. That gives
the view ray `ray = normalize(farPoint - camera)`.

```
above the horizon: mix(horizon, zenith, sqrt(ray.y))         # sqrt: the horizon band is thin
below:             mix(horizon, ground, saturate(-ray.y * 4))
sun disc + glow:   sunColor * (t^800 * 6 + t^12 * 0.25),  t = max(dot(ray, sunDir), 0)
```

Thick fog also hides the sky: `mix(sky, fogColor, saturate(density * 25))`.

## 6. The packed block

`LightingBlock` is 160 floats and the same in C++, HLSL and WGSL:

| field          | xyz                            | w                       |
|----------------|--------------------------------|-------------------------|
| cameraPosition | camera                         | seconds (flicker clock) |
| sunDirection   | towards the sun or moon        | 1 = casts shadows       |
| sunColor       | color                          | –                       |
| skyColor       | zenith                         | ambient strength        |
| horizonColor   | horizon (also the fog color)   | –                       |
| groundColor    | ground bounce                  | –                       |
| fogColor       | color                          | density per meter       |
| counts         | x = lights, y = 1 / shadow map size | –                  |
| lightPositions[16] | position                   | range                   |
| lightColors[16]    | color × intensity × flicker | –                      |
