# ComfyUI workflows for Umbra

Umbra's AI tools talk to a ComfyUI on this computer (`engine/localai/include/LocalAi.md`).
Textures (P8C) need nothing here: their small text-to-image graph is built in code.

## Your exported image-to-3D workflow (AI models, P8D)

The image-to-3D graph is large (66 nodes) and changes with ComfyUI updates, so Umbra runs
**your own export** of ComfyUI's official template instead of a copy kept here. Once:

1. Update ComfyUI (the native TRELLIS.2 nodes arrived in August 2026).
2. In ComfyUI: **Templates** → search "TRELLIS.2" → **Pixal3D & TRELLIS.2: Image to Model**.
3. Download the models its note lists (about 10 GB), into the folders it names.
4. Set the switch node ("Switch Model Here") to **true** = TRELLIS.2 (MIT license; user decision).
5. Run it once with any picture so you know it works on your GPU (optional).
6. **Workflow → Export (API)** and save the file in this folder, under **any name**. The editor's
   AI model window lists every usable export here and remembers your choice; if there is a file
   named `image-to-3d.api.json` it is the default.

Faster on an 8 GB GPU (measured 2026-11-07): set the **Trellis2 Upsample Stage** `target_resolution`
to 1024 (1536 spills out of VRAM: 58 s per step instead of ~1.5 s) and **Remesh Mesh** `resolution`
to 512. Close the game and other GPU programs while it runs.

What Umbra changes in it for each run (`localai::PrepareModelWorkflow`):

| Node | Input | Set to |
|---|---|---|
| `LoadImage` (exactly one) | `image` | the uploaded picture |
| every sampler | `seed` / `noise_seed` | the run's seed, +1 per sampler |
| `DecimateMesh` | `target_face_count` | 10 000 (the editor's Triangles slider) |
| `BakeTextureFromVoxel` / `UnwrapMesh` | `texture_size` / `resolution` | 1024 (also through a Primitive node wired into them) |

Umbra also resolves fixed switches and runs only what the node that saves a GLB (`Save 3D
(Advanced)` or `SaveGLB`) needs: the template's preview models are skipped and the GLB is taken from
that node. Any other image-to-3D workflow works too (e.g. Hunyuan3D) if it has one `LoadImage` and
saves a `.glb`.

The exported file is yours: it names model files on your PC, so it isn't committed for you.
