# P8D — AI 3D models through ComfyUI: verification record

## Record

- Date: 2026-11-07
- Commit: `issue #UMBRA-61` (P8D.1–P8D.4)
- Tester: Claude (remote session), Linux cloud workspace.
- Update 2026-11-08: **verified on Windows by the user (works, with limitations; fixed in `#UMBRA-62`, §3c).**
- State (2026-11-07): **implemented. All automated checks pass against a stand-in ComfyUI (which serves the
  repository's `wolf.glb` as the "generated" model). No real TRELLIS.2 run yet; the editor window
  has not been opened.**

User decisions (2026-11-07, ADR 0003): props first; ~10 000 triangles; text or own picture; TRELLIS.2.

## What was built

| Task | Where |
|---|---|
| P8D.1 Bridge | `ComfyUi.hpp`: `LoadModelWorkflow` (API format, one `LoadImage`, a 3D save node), `PrepareModelWorkflow` (picture, seeds, `DecimateMesh`, texture size incl. Primitive nodes), `GenerateModel` (upload, run, download the GLB); shared `RunGraph`; `BatchJob` template (`ImageJob`, `ModelJob`) |
| P8D.2 Assets | `ModelAssistant.hpp`: object style for concepts, `InspectModel` (importer: triangles, meshes, texture size, bounds), `SaveGeneratedModel` (GLB + concept + provenance, removed again if the importer refuses it), `PlacedModelEntity`; `ModelLicense` knows TRELLIS.2 (MIT) |
| P8D.3 Editor and tool | Editor **AI model** window (concepts with "View", own picture, triangles, Make 3D model, Place in scene = one undoable edit); `UmbraTool model` |
| P8D.4 Records | ADR 0003, `ModelGen.md`, `tools/comfyui/README.md` (how to export the workflow), creator guide §2d, this record |

## 1. Automated checks (Linux)

| Configuration | Result |
|---|---|
| GCC 13, Debug | 43/43 CTest entries pass |
| Clang 18, Debug | 43/43 |
| GCC + ASan/UBSan | 43/43; `UmbraLocalAiTests` repeated 3× more: pass |

New: 6 model test cases in `ComfyUiTests.cpp` (64 assertions) and `Umbra.Tool.ModelNeedsWorkflow`.
ASan caught nothing; a use-after-free in the provenance code (`items()` of a temporary JSON in a
range-for) was found by a crash in the first test run and fixed before commit.

## 2. Test cases

| Test | Evidence |
|---|---|
| T67 workflow problems explained | missing file (`workflow.missing`), UI format (`workflow.format`, names Export (API)), no/two `LoadImage` (`workflow.image`), no 3D save (`workflow.output`) |
| T68 run values applied, loaded workflow untouched | picture name, seeds 1000/1001, faces 10 000, 1024 through literal and Primitive inputs |
| T69 upload → run → download | PNG uploaded as multipart with `type=input`; the GLB listed under the save node's `3d` output downloaded byte-identical; missing model → `ai.model` naming `unet_name`; non-local → `ai.host` |
| T70 saved model is usable or not kept | GLB, concept, provenance (seed, prompt, TRELLIS.2 = MIT, commercial); second save `_2`; importer counts > 1000 triangles; broken bytes → `model.import`, all files removed |
| Placement | 1 m model scaled to 2 m stands on the ground (y lifted by −min·scale), free id `barrel_2`, `instance` for several meshes |
| T66 game without generation code | unchanged: `Umbra.GameExcludesEditor` |

## 3. Limits, honestly

- No real TRELLIS.2 run: quality, time (likely minutes) and whether it fits an 8 GB GPU are unknown.
- Concept pictures open in the system image viewer, not inside the editor.
- Facing direction is as generated. Placed models get a fitted box collider (`#UMBRA-63`); colliders have no offset, so it fits centered models (TRELLIS.2's are).
- Characters, rigging and animation are out of scope (ADR 0003).

## 3c. Windows verification and fixes (2026-11-07/08, user; `#UMBRA-62`)

The user exported the template (as `3d_pixal3d_trellis2_image_to_model.json`, switch = TRELLIS.2),
generated models on their 8 GB GPU and placed them; they report P8D **works, with limitations**:

| Limitation reported | Cause | Fix (`#UMBRA-62`) |
|---|---|---|
| Only `image-to-3d.api.json` was looked for | fixed default name | Any usable export in `tools/comfyui` is found (`FindModelWorkflows`); the editor lists them and remembers the choice (`ai-model.json` in preferences) |
| "Place in scene" gave a white model; the right GLB had to be taken from ComfyUI's zip | the first `.glb` in the history was downloaded: the template's untextured voxel preview (POSITION only), not the textured `Save 3D` output | the GLB is taken from the save node's output only; and the graph is reduced to what the save node needs (switches resolved, preview models and the unused Pixal3D branch not run, so faster) |
| The job should survive closing windows/editor | windows were fine (jobs belong to the editor), but closing the editor sent `/interrupt` | closing the editor leaves ComfyUI working; the job id is written down and the next start fetches and saves the model; `UmbraTool model --resume <job>` |
| "nothing produced after 1800 s" | a fixed 30-minute limit | no total limit: waiting stops only if ComfyUI loses the job (`ai.lost`, checked in `/queue`) or stops answering for 2 minutes |

Timing from the user's first run (Pixal3D branch, upsample 1536): structure 18 s, shape 24 s,
high-res shape **11 min 35 s** (58 s/step: VRAM spill on 8 GB). The user's export now uses
upsample 1024 and remesh 512.

Automated after the fixes: 43/43 CTest on GCC, Clang and GCC ASan/UBSan; the test workflow is now
the user's real export. New: T71 (graph reduced to the save node; GLB from the save node), T72
(leaving keeps ComfyUI working; resume by job id; no time limit while ComfyUI has the job; lost
job named).

## 4. Windows checklist (user: works, 2026-11-08)

| # | Do | Expect | Result |
|---|---|---|---|
| 1 | `scripts\build-and-run.bat` | 44 CTest entries pass (43 + the Windows-exception test) | |
| 2 | Export the TRELLIS.2 workflow (`tools/comfyui/README.md`) after one successful run in ComfyUI | `tools\comfyui\image-to-3d.api.json` exists; note ComfyUI's time and VRAM | |
| 3 | Editor → **AI model**: "wooden barrel with iron bands", 4 concepts, **View** each | Pictures of one object on a plain background | |
| 4 | Pick one → **Make 3D model** | Note minutes; a `models\...glb` appears with ~10 000 triangles, 1024 px texture | |
| 5 | Height 1.2 → **Place in scene**, Ctrl+Z, Ctrl+Y, Ctrl+S, F5 | Barrel stands on the ground; undo/redo; visible in the game | |
| 6 | **Use my picture** with a PNG of your own | Same, from your picture | |
| 7 | Cancel during "Making the 3D model" | "Cancelled." at once; ComfyUI stops | |
| 8 | `UmbraTool model --idea "stone lantern" --assets assets` | Concept, model, triangles printed | |
