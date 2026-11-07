# AI models (`ComfyUi.hpp` 3D part, `ModelAssistant.hpp`)

Roadmap P8D. A prop goes from words (or a picture) to a model in a scene in four steps:

```text
"mossy barrel" --(SDXL, object style)--> concept pictures --(pick one)--+
my_drawing.png ----------------------------------------------------------+--> upload (/upload/image)
   --> the creator's exported image-to-3D workflow (TRELLIS.2), with picture/seed/faces/texture size set
   --> /history: the output listing a .glb --> /view: download
   --> models/<name>.glb + <name>.concept.png + <name>.source.json --> importer check --> Place in scene
```

## Why the creator's own workflow

ComfyUI's official "Pixal3D & TRELLIS.2: Image to Model" template has 66 nodes (background
removal, structure, shape, upsample and texture samplers, remeshing, decimation, UV unwrapping,
texture baking). Copying it into Umbra would break with every ComfyUI update. Instead the creator
exports it once with **Export (API)** (`tools/comfyui/README.md`) and Umbra only changes four
things (`PrepareModelWorkflow`): the `LoadImage` picture, the sampler seeds (run seed +1 each), the
`DecimateMesh` face count and the texture bake/unwrap size, also when these come from a Primitive
node wired into the input. `LoadModelWorkflow` refuses a UI-format file (with how to export it),
a workflow without exactly one `LoadImage`, and one that saves no 3D file.

TRELLIS.2 outputs about 700 000 faces by default; Umbra asks for 10 000 (user decision
2026-11-07, like the ~8 000-triangle Meshy characters) and 1024 px textures.

## Saving

`SaveGeneratedModel` writes new files only (`_2`, `_3`...), through temporary files:

| File | What |
|---|---|
| `models/<name>.glb` | the model |
| `models/<name>.concept.png` | the picture it was made from |
| `models/<name>.source.json` | prompt or picture name, workflow file, seed, faces, date, and every model file the workflow loads with its license (`assets::ModelLicense`; TRELLIS.2 = MIT) |

Then the game's own importer reads the GLB; if it can't, all three files are removed again
(`model.import`). Unknown or non-commercial licenses become `material.license` warnings.

## Placing

`PlacedModelEntity` makes a scene entity with a free id: scaled so the model is `height` meters
tall (bounds from all mesh vertices through the node transforms, `InspectModel`), lifted so its
lowest point touches the ground, `"model"` + `"mesh": 0` for one mesh or `"instance"` for several.
No collider is added: generated shapes vary, so the creator adds one in the inspector.

## Limits

- Image-to-3D models expect one object on a plain background; the object style
  (`objectPrompt`/`objectNegative` in `assets/materials/style.json`) asks SDXL for that.
- The editor can't show pictures inside its windows yet; concept pictures open in the system
  image viewer ("View"), saved under the preferences folder (`ai-concepts/`).
- Facing direction is whatever the model came out with; turn it with the gizmo.
