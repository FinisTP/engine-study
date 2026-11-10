# Custom surface shaders (S7)

A material can give its surface its own look: rippling water, a pulsing crystal, scrolling lava.
You write one small HLSL function, and the engine puts it inside its scene shader, so lighting,
shadows, fog and post-processing still apply.

## 1. Writing one

The file is `assets/shaders/<name>.surface.hlsl`, in the project or in a plugin. `<name>` may use
letters, digits and `_`.

```hlsl
// shaders/water.surface.hlsl
void Surface(inout SurfaceData s, SurfaceInput i)
{
    const float a = sin(i.worldPosition.x * 3.0F + i.time * 1.7F);
    s.normal = normalize(i.normal + float3(a, 0.0F, 0.0F) * 0.08F);
    s.albedo = SampleBase(i.uv + a * 0.01F).rgb * i.tint.rgb;
}
```

| `SurfaceInput i` | |
|---|---|
| `uv` | texture coordinates (world-space ones if the material repeats by meters) |
| `worldPosition` | meters |
| `normal` | unit length, world space |
| `time` | seconds since the game started |
| `tint` | the entity's color times the mesh color |

| `SurfaceData s` (starts as the plain material) | |
|---|---|
| `albedo`, `alpha` | color; plain = texture × tint |
| `normal` | change it for ripples or bumps; keep it unit length |
| `emissive` | glow added to the light; plain = the entity's `emissive` |

You can also use `SampleBase(uv)` (the material's texture at any coordinates) and `scene.*` (the frame
data, e.g. `scene.cameraPosition.xyz`). Both are in `Surface.hlsli`.

The material names the file:

```json
{ "format": "umbra.material", "version": 1, "baseColor": "textures/water.png",
  "projection": "world", "metersPerRepeat": 1.0, "shader": "shaders/water.surface.hlsl" }
```

## 2. How it is built

When CMake configures, it finds every `*.surface.hlsl` (`engine/render/SurfaceShaders.cmake`) and
writes a small wrapper for each:

```hlsl
#include ".../Surface.hlsli"      // structs, resources, SampleBase
#define UMBRA_SURFACE 1           // Scene.hlsl then skips its empty Surface()
#include ".../water.surface.hlsl" // your function
#include ".../Scene.hlsl"         // PSMain calls Surface()
```

- **Vulkan.** DXC compiles the wrapper's `PSMain` to `shaders/surface/water.frag.spv`. The renderer
  makes one extra scene pipeline per file it finds there (two since S8: plain and skinned, the
  skinned one with `VSSkinned` from `Skinned.vert.spv`). Draws whose material names the shader use
  that pipeline; all the others use the plain one.
- **Browser.** The same wrapper is compiled with `-DUMBRA_WEB` (the browser's bindings; see
  `Surface.hlsli`) to SPIR-V, and **naga** turns that SPIR-V into WGSL (`cargo install naga-cli`).
  The `.wgsl` files are embedded at `/shaders/surface`, and `RendererWeb` makes a WebGPU pipeline
  from each one, using the browser's own vertex shader. That is why the browser's vertex outputs use
  the same locations as `Scene.hlsl`'s `VertexOutput` (0 normal, 1 uv, 2 color, 3 world position,
  4 emissive).

Adding a new shader file needs a build, but no C++ change: CMake notices the new file when it
re-checks its file lists.

## 3. Mistakes

A shader with an error stops the build. DXC names the surface file and line, for example
`water.surface.hlsl:5:18: error: use of undeclared identifier 'undefinedThing'`, because the wrapper
includes the file instead of copying it. The test `Umbra.Render.SurfaceShaderErrors` (T86) checks this.

A material that names a missing file, or a name without `.surface.hlsl`, is refused when the scene
loads (`material.shader`). If a pipeline is missing at run time, for example because the shader was
added after the build, the object is drawn with the plain surface.
