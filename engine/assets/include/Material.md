# Materials (`Material.hpp`)

A material gives a scene object its own surface instead of the texture inside its model file
(roadmap P8C.1). One small JSON file per look:

```json
{ "format": "umbra.material", "version": 1,
  "baseColor": "textures/sample_bricks.png",
  "projection": "world",
  "metersPerRepeat": 2.0,
  "tint": [1, 1, 1, 1],
  "generated": { "provider": "comfyui", "model": "sd_xl_base_1.0.safetensors",
                 "license": "CreativeML OpenRAIL++-M", "prompt": "...", "negative": "...",
                 "seed": 1234, "steps": 25, "date": "2026-11-08" } }
```

| Field | Meaning |
|---|---|
| `baseColor` | Image under the asset folder. PNG/JPEG/BMP, **power-of-two sides from 64 to 2048** (mipmaps, GPU limits) |
| `projection` | `"uv"`: the model's own texture coordinates. `"world"`: repeat by distance, every `metersPerRepeat` meters (`engine/render/shaders/WorldUv.md`) |
| `metersPerRepeat` | 0.05–100; only used by `"world"` |
| `tint` | Multiplied with the image and the entity's `color` |
| `generated` | Optional: how an AI texture was made (make it again; check the license) |

A scene entity uses it with `"material": "materials/x.material.json"` (it needs a `model` to
cover). The scene loader, `UmbraTool validate` and the editor all run `LoadMaterial` +
`CheckMaterialImage`, so a missing or badly sized image is refused before the game shows it
(codes `material.*`, `asset.missing`). The game and editor load materials through
`ModelLibrary::ResolveMaterial`; editing the material or its image while the game runs reloads it.

## Licenses

`ModelLicense(checkpoint)` looks the model up by file name (SDXL, SD 1.5, SD 3.5, FLUX.1 schnell/dev,
FLUX.2 klein 4B/9B, Z-Image). `CheckMaterialLicense` warns (`material.license`) when a generated
texture's model is non-commercial or unknown; `scripts/package.bat` validates every material and
prints those warnings. The table is a best effort, not legal advice: read the model's license.
