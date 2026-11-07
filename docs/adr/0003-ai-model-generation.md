# ADR 0003: AI 3D models through ComfyUI (roadmap P8D, decision O17)

- Status: **accepted; implemented** (2026-11-07, `#UMBRA-61`, evidence `../testing/p8d-ai-models.md`).
  The real TRELLIS.2 run on the user's PC is still open.
- Context: the reference characters came from Meshy (cloud, paid credits, `#UMBRA-52`). The user
  pointed out that ComfyUI, already installed for textures (P8C), now generates 3D models: TRELLIS.2
  and Pixal3D became native ComfyUI nodes on 2026-08-22, with no custom nodes and no
  non-commercial dependencies; TRELLIS.2's code and weights are MIT.

## Decisions (user, 2026-11-07)

| Choice | Decision |
|---|---|
| First use | Props and objects (static). Characters stay with Meshy until rigging exists |
| Triangle budget | About 10 000 per model (TRELLIS.2 makes ~700 000; reduced inside ComfyUI by `DecimateMesh`) |
| Input | Text (→ SDXL concept pictures → pick one) or the creator's own picture |
| 3D model | TRELLIS.2 (MIT) |

## Design

1. **The creator's exported workflow, not a copy.** The official template is a 66-node graph that
   needs ~10 GB of models and changes with ComfyUI updates. The creator exports it once with
   Export (API) to `tools/comfyui/image-to-3d.api.json`; Umbra changes only the picture, seeds, face
   count and texture size (`localai::PrepareModelWorkflow`). Any image-to-3D workflow with one
   `LoadImage` and a GLB save node works.
2. **Same bridge as textures:** local addresses only, upload → queue → wait → download through one
   shared `RunGraph`, cancel with `/interrupt`, named failures, one `BatchJob` template.
3. **Assets with provenance:** `models/<name>.glb` + concept picture + `<name>.source.json` (prompt,
   seed, faces, model files and licenses); the game's importer must accept the GLB or nothing is kept.
4. **Placing is an ordinary undoable scene edit** (height in meters, standing on the ground).

## Rejected

- Building the TRELLIS.2 graph in code: brittle across ComfyUI versions, and untestable here.
- Python post-processing like `tools/meshy/slim_glb.py`: ComfyUI's own decimation and texture
  baking already make game-sized models, so the C++ side only checks and places them.
- Showing concept pictures inside the editor: the overlay renderer draws only its font texture
  today. Concepts open in the system image viewer; in-editor thumbnails are a later improvement.
