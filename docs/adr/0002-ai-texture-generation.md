# ADR 0002: AI texture generation (roadmap P8C, decision O16)

- Status: **proposed** (2026-11-07). The user asked for it; it isn't in the confirmed scope yet,
  and the choices in §7 still need answers.
- Context: Umbra's AI content tools so far draft quests and dialogue (P8B), and the reference
  characters were made with Meshy (`#UMBRA-52`). The world itself still uses a 2×2 placeholder image
  stretched over cubes. Creators should be able to make surface textures (stone, wood, grass, roof,
  cloth...) with an image model running **on their own PC**, just as quests use a local Ollama.

## 1. Where the engine stands (checked 2026-11-07)

| Need | Today | Gap |
|---|---|---|
| A texture on a scene object | Only what's inside the object's glTF file (`ModelMesh::texture`) | No way to give a scene entity its own texture |
| Repeating a texture on big boxes, walls, floors | Box UVs are 0–1 per face, so the texture stretches; the sampler already repeats | World-scale texture coordinates |
| Loading a PNG on its own | `stb_image` is already in the build (it comes with tinygltf), but only used for glTF images | A standalone image import into the asset cache |
| Distant surfaces | One mip level, so repeated textures shimmer | Mipmaps |
| Normal/roughness maps | The shader is textured Lambert with base color only | PBR renderer (later; not part of this plan) |

So half of this plan is engine work that pays off even for hand-made textures.

## 2. Which generator (researched 2026-11-07)

| Option | For | Against |
|---|---|---|
| **ComfyUI** (desktop app or portable Windows build) | The most-used local Stable Diffusion/Flux runner; HTTP API (`/prompt`, `/history`, `/view`); any model (SD 1.5, SDXL, Flux, Z-Image...); workflows are JSON files we can keep in the repo | Node graphs are complex (we'd ship the workflow, the creator never sees it); seamless tiling needs a custom node (e.g. tiled_ksampler) |
| **Forge** (AUTOMATIC1111 successor) | Simple form-style API (`/sdapi/v1/txt2img`); a tiling option | Less actively developed than ComfyUI; mostly SD 1.5/SDXL |
| **Ollama image generation** | Same program as P8B; `/api/generate` | Experimental since January 2026; macOS first, Linux (CUDA) later. Windows support wasn't confirmed when this was written, so recheck in P8C.0 |
| **stable-diffusion.cpp** built into Umbra | No separate program | Puts a large GPU dependency into our build and owns model downloads; against "lightweight engine" |

**Proposal:** one small `ImageProvider` interface in `Umbra::LocalAi`, the first adapter for
**ComfyUI**, and Ollama added as a second adapter once it generates images on Windows. Like P8B,
it accepts only addresses on this computer and never falls back to a cloud service.

## 3. Seamless (tileable) textures

A surface texture has to repeat without visible seams. Two layers:

1. **In the generator, when it can:** circular padding in the model's convolution layers (the
   "tiling" option in Forge, a tiling sampler node in ComfyUI). Turning it on only for the last
   steps keeps quality better.
2. **In Umbra, always:** a small C++ step that works with any generator. Shift the image by half its
   size so the seams move to the middle, then cross-fade them with a soft mask. After that, a
   **seam score** measures the difference across the wrap-around edges, so the editor can warn
   about a visible seam. Math in a markdown file beside the code (project rule).

## 4. Where generated textures live

```text
assets/textures/stone_wall.png          the image (power of two, at most 2048 px)
assets/materials/stone_wall.material.json
{ "format": "umbra.material", "version": 1,
  "baseColor": "textures/stone_wall.png",
  "projection": "world",                 // "uv" = the model's own UVs; "world" = repeat by meters
  "metersPerRepeat": 2.0,
  "tint": [1, 1, 1, 1],
  "generated": { "provider": "comfyui", "model": "sdxl_base_1.0", "license": "openrail++",
                 "prompt": "...", "negative": "...", "seed": 1234, "steps": 25, "date": "2026-11-08" } }
```

- A scene entity gets an optional `"material": "materials/stone_wall.material.json"`, validated
  like every other reference (the game's loader, `UmbraTool validate`, editor).
- The `generated` block records how the texture was made, so it can be made again and its
  license checked. `package.bat` warns about non-commercial models (e.g. FLUX.1 dev,
  FLUX.2 klein 9B) and quietly accepts permissive ones (Apache-2.0 FLUX.1 schnell, Z-Image-Turbo,
  FLUX.2 klein 4B; SDXL's OpenRAIL++). The license table is data, checked again when it changes.
- The game only loads finished PNG and material files. Players never run an image model (same rule
  as C08: creators generate, players get fixed assets).

## 5. Creator workflow (editor)

1. Select an object (a wall, the ground) → **AI texture**.
2. Type what it should look like: "mossy cobblestone". A project-wide **style** (prompt prefix and
   negative prompt in `assets/materials/style.json`, e.g. "stylized hand-painted, low-poly game
   texture") keeps every texture in the look of the Meshy characters.
3. **Generate 4** variations. Each shows as a tiled 3×3 preview with its seam score, while the
   editor keeps running (worker thread, Cancel, as in P8B).
4. Pick one → **Apply**. The PNG and material file are written first (new files, atomically), then
   the entity's `material` is set as one undoable edit. Undo removes the reference; the files stay
   so redo works, and the asset browser can list unused ones.
5. Variations: "more like this one" (same seed, changed prompt), or a different seed.

`UmbraTool texture <prompt> --out materials/x.material.json [--seed n] [--size 1024]` does the same
for scripts and coding agents.

## 6. Plan (proposed tasks)

| Task | Implement | Evidence |
|---|---|---|
| P8C.0 Spike | Install the chosen generator on the user's PC; measure time per 1024² texture, VRAM, quality with tiling on/off; recheck Ollama image generation on Windows | Recorded numbers and sample images; choice of default model |
| P8C.1 Engine textures | Standalone PNG import into the asset cache with mipmaps; `umbra.material` files; scene `material` field with validation and hot reload; world-scale projection (shader push constant); editor material picker | Asset/scene tests; tiled cubes in the settlement render without stretching (Windows check) |
| P8C.2 Image provider | `ImageProvider` + ComfyUI adapter: workflow template in `tools/`, queue, poll, download, list installed models; local only; cancel, size and time limits | Stand-in ComfyUI server tests (as `tests/localai`), failures named (`ai.*`) |
| P8C.3 Texture checks | Seamless step, seam score, power-of-two size, provenance block, license table | Unit tests on known images (a seamed one must score worse) |
| P8C.4 Editor and tool | AI texture window (variations, tiled preview, apply/undo, style), `UmbraTool texture` | Windows checklist: make and apply a wall texture, undo, play |
| P8C.5 Records | Creator guide section, testing record, roadmap | Docs |

New test cases: **T61** a generated texture with a seam is flagged; **T62** apply → undo → redo
leaves the scene and files consistent; **T63** a material referencing a missing/oversized PNG is
refused by the loader; **T64** no non-local generator address; **T65** non-commercial model
license is warned about at packaging; **T66** the game build links no image-generation code.

**Not in this plan:** normal/roughness/height maps (they need the PBR renderer; when that
comes, a height-from-luminance → normal step can fill them), retexturing characters (Meshy's
retexture or ComfyUI with the model's UVs), item icons, textures generated while playing.

## 7. Choices for the user

1. Generator program: ComfyUI (proposed), Forge, or wait for Ollama image generation on Windows.
2. What first: tileable world surfaces only (proposed), or also item icons.
3. GPU memory, which decides the default model: about 6 GB → SD 1.5 at 512 px; 8 GB → SDXL at
   1024 px; 12–16 GB → FLUX.1 schnell or Z-Image-Turbo.
4. When: an independent track before the P9 release gate (proposed), or after it.
