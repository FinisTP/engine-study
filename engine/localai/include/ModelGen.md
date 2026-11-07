# AI models (`ComfyUi.hpp` 3D part, `ModelAssistant.hpp`)

Roadmap P8D. A prop goes from words (or a picture) to a model in a scene in four steps:

```text
"mossy barrel" --(SDXL, object style)--> concept pictures --(pick one)--+
my_drawing.png ----------------------------------------------------------+--> upload (/upload/image)
   --> the creator's exported image-to-3D workflow (TRELLIS.2), reduced to what its save node needs,
       with picture/seed/faces/texture size set
   --> /history: the save node's .glb --> /view: download
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
a workflow without exactly one `LoadImage`, and one that saves no 3D file. The file may have any
name: `FindModelWorkflows` lists the usable exports in `tools/comfyui`; the editor remembers the
choice.

## Only what the save node needs

ComfyUI runs every output node in a graph. The template has three: two **preview** models (an
untextured voxel mesh and a vertex-painted 700 000-face mesh) and the **Save 3D** node with the
finished, textured, decimated model. Before `#UMBRA-62` Umbra downloaded the first `.glb` in the
history, which was the white voxel preview. Now `PrepareModelWorkflow`:

1. **Resolves switches.** A `ComfySwitchNode` whose choice is fixed (a literal, or a Primitive
   boolean such as "Switch to Trellis2") is removed and everything reading it reads the picked
   input instead.
2. **Keeps only the save node and what it reads**, directly or indirectly (a walk over the node
   links). The previews, the UV-atlas preview and the unused branch (Pixal3D's model, MoGe, its
   conditioning) are dropped, so ComfyUI does less work and the provenance lists only models that ran.
3. Downloads the `.glb` listed under **the save node's** output.

## Long runs and closing the editor

A run can take many minutes on an 8 GB GPU. There is no total time limit: while ComfyUI still has
the job (`/queue` lists it as running or waiting) Umbra keeps waiting. It stops with `ai.lost` if
ComfyUI forgot the job (restarted, queue cleared), or `ai.connect` if ComfyUI stops answering for
`timeoutSeconds` (2 minutes).

Closing the editor uses `CancelToken::Leave`: Umbra stops waiting but does not send `/interrupt`, so
ComfyUI finishes. The job id was written to `ai-model-pending.json` in the preferences folder
(with the name, seed, workflow and the picture as `ai-model-pending.png`) as soon as ComfyUI accepted
it; the next editor start fetches the result by id (`ModelRequest::resumeId`, nothing queued again)
and saves it as usual. **Cancel** still interrupts ComfyUI. `UmbraTool model` prints the job id and
`UmbraTool model --resume <job> --assets <folder>` fetches it later.

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
It gets a static box collider fitted to the model (`FittedCollider`): half the model's size on each
axis times the entity scale, in meters. The box is centered on the entity origin, which fits
TRELLIS.2 models (centered in a unit cube; the user's well: -0.475..0.476). A model whose origin
is far from its middle (e.g. a character with its feet at the origin) would get a shifted box:
colliders have no offset yet. Before `#UMBRA-63` creators had to add a collider by hand, and its default
size came from the entity scale (meant for the -1..1 sample cube), so a 1 m model at scale 5 got a
10 m box.

## Limits

- Image-to-3D models expect one object on a plain background; the object style
  (`objectPrompt`/`objectNegative` in `assets/materials/style.json`) asks SDXL for that.
- The editor can't show pictures inside its windows yet; concept pictures open in the system
  image viewer ("View"), saved under the preferences folder (`ai-concepts/`).
- Facing direction is whatever the model came out with; turn it with the gizmo.
