# P8D — AI 3D models through ComfyUI: verification record

## Record

- Date: 2026-11-07
- Commit: `issue #UMBRA-61` (P8D.1–P8D.4)
- Tester: Claude (remote session), Linux cloud workspace.
- State: **implemented. All automated checks pass against a stand-in ComfyUI (which serves the
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
- No collider is added on placement; facing direction is as generated.
- Characters, rigging and animation are out of scope (ADR 0003).

## 4. Windows checklist (open)

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
