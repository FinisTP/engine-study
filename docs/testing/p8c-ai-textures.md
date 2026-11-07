# P8C — AI texture generation: verification record

## Record

- Date: 2026-11-07
- Commit: `issue #UMBRA-59` (P8C.1–P8C.5)
- Tester: Claude (remote session), Linux cloud workspace.
- State: **implemented. Every automated check passes, against a stand-in ComfyUI server and
  generated test images. No real ComfyUI or SDXL run yet (the workspace can't download models or
  reach the user's PC). Rendering changes (mipmaps, world-space texture coordinates) compile but have
  not run on a GPU; the editor window has not been opened.**

User decisions (2026-11-07, ADR 0002 §7): ComfyUI first; tileable world surfaces; SDXL base 1.0
at 1024 px for the user's 8 GB GPU; before P9.

## What was built

| Task | Where |
|---|---|
| P8C.1 Engine textures | `core::MipChain` (`MipMaps.md`) and all mip levels uploaded with `maxLod` (every texture, glTF ones too); `DrawItem::metersPerRepeat` → push constant → world-space UVs in `Scene.hlsl` (`WorldUv.md`); `assets::Material` files (`Material.md`); scene `material` field (validated with the material's own messages, saved back); `ModelLibrary::ResolveMaterial` with hot reload; editor Material picker; sample `materials/sample_bricks.material.json` (hand-drawn by `tools/textures/make_sample_bricks.py`) on the settlement's low wall and house A |
| P8C.2 Image provider | `localai::GenerateImage` (ComfyUI `/prompt`, `/history`, `/view`, `/interrupt`), `TextureWorkflow`, `ListCheckpoints` (both list formats), `FindComfyUi` (ports 8000, 8188), `ImageJob` |
| P8C.3 Texture checks | `assets::MakeSeamless`, `SeamScore`, `Resize`, PNG encode/decode, size rule; provenance (`generated` block) and `ModelLicense` table; `UmbraTool validate` checks materials; `package.bat` validates every material and prints license warnings |
| P8C.4 Editor and tool | AI texture window (`apps/editor/AiTexture.cpp`): variations previewed on the selected object, apply = files first, then one undoable edit; `UmbraTool texture`; `assets/materials/style.json` |
| P8C.5 Records | creator guide §2c, Editor.md, LocalAi.md, TextureTools.md, SceneFormat.md, UmbraTool README, this record |

## 1. Automated checks (Linux)

| Configuration | Result |
|---|---|
| GCC 13, Debug | 42/42 CTest entries pass |
| Clang 18, Debug | 42/42 |
| GCC + ASan/UBSan | 42/42; `UmbraLocalAiTests` repeated 3× more: pass |
| `UmbraLocalAiTests` repeated 8× (GCC) | all pass |

New tests: `TextureToolsTests` (6 cases), 2 scene material cases, 1 mip chain case, `ComfyUiTests`
(9 cases against `FakeComfyUi`: queue → history after two polls → `/view`; missing model; failed
run; too-large picture; timeout with interrupt; non-local address; nothing listening; cancel with
interrupt; both checkpoint list formats; batch of variations, stopping at a failure; save with
provenance; style). CTests `Umbra.Tool.TextureLocalOnly`, `Umbra.Tool.ValidateMaterial`.

## 2. Roadmap test cases

| Test | Evidence |
|---|---|
| T61 seam flagged | a left-to-right ramp scores above 2 (`visibleSeam`); after `MakeSeamless` below 2 and less than a tenth of before; an exactly repeating wave scores below 1.5 |
| T62 apply/undo/redo | `SaveTextureMaterial` writes the image, then the material, each via a temporary file and rename, never over an existing name (`_2`); the scene edit is one ordinary undoable document edit (P7 tests cover undo/redo). **Editor flow itself not run** |
| T63 bad material image | missing file (`asset.missing`), 100×100 image (`material.image`, message names the size), material without model (`entity.material.nomodel`); the scene isn't loaded, the world stays empty |
| T64 local only | `ai.host` for a non-local ComfyUI address before connecting (library and `Umbra.Tool.TextureLocalOnly`) |
| T65 license warning | `ModelLicense` table; `flux1-dev` → warning `material.license`; SDXL and hand-made materials → none; `package.bat` prints the warnings |
| T66 game without generation code | `Umbra.GameExcludesEditor` forbids `UmbraLocalAi` and `httplib`; `Umbra.EngineOffline` keeps the engine modules off the network. The game does link the small image tools in `Umbra::Assets` (decode, resize) but no generator |

## 3. Limits, honestly

- No real ComfyUI/SDXL run: picture quality, time per texture and VRAM use on the user's PC are
  unknown (that's P8C.0, below).
- `MakeSeamless` blends the middle band of the picture; on strongly patterned textures (bricks,
  tiles) the blend can show as a soft double image. Circular padding inside the model
  (ComfyUI custom nodes) would be better and can be added as a second workflow later.
- World-space texture coordinates pick one plane per vertex: right for boxes, walls and floors,
  wrong for curved models (those keep `"projection": "uv"`). Textures stay fixed in the world, so a
  moving object would slide through its texture.
- Mipmaps average sRGB values directly (slightly dark small levels on high-contrast images).
- Only base-color textures: normal/roughness maps wait for a PBR renderer.

## 3b. First Windows builds (2026-11-07, user)

- Build 1: stopped after ~100 of 227 steps with "CreateProcess: Access is denied" for `cmake.exe`
  (security software); all new P8C code compiled without errors up to there.
- Build 2: `LNK1104: cannot open file UmbraSceneCheck.exe` (file locked) stopped ninja, but
  `build-and-run.bat` printed BUILD OK because the failure exit code was -1 and the script used
  `if errorlevel 1` (true only for codes >= 1). The tests then ran on the old UmbraTool/editor:
  8 failures, none from new code (5 SceneCheck "Not Run", Gameplay blocked from starting,
  TextureLocalOnly/ValidateMaterial = old UmbraTool). Fix `#UMBRA-60`: the scripts treat any exit
  code other than 0 as failure.

## 3c. Windows verification (2026-11-08, user)

The user ran the checklist below on their PC and reported that **everything works properly, with
some limitations** (P8B, P8C and P8D confirmed together). Item-by-item results were not reported, so
the rows below are left blank; the milestone counts as verified on Windows by the user's report.

First real texture: `an_old_water_well` (SDXL base 1.0, seed 47429632), in the user's working copy.

## 4. Windows checklist (user: works, 2026-11-08)

| # | Do | Expect | Result |
|---|---|---|---|
| 1 | `scripts\build-and-run.bat` | 43 CTest entries pass (42 + the Windows-exception test) | |
| 2 | Play, look at the settlement's low wall and house A | Bricks repeat at real size (about 2 m per repeat), not stretched; no shimmer far away (mipmaps) | |
| 3 | P8C.0: install ComfyUI Desktop, put `sd_xl_base_1.0.safetensors` in `models/checkpoints`, start it | ComfyUI's own window works | |
| 4 | Editor: select a wall → **AI texture**; the model list shows SDXL | Found on port 8000 (or 8188) | |
| 5 | "mossy cobblestone", 4 variations, **Generate** | Note seconds per picture and GPU memory (Task Manager → GPU); the editor never freezes | |
| 6 | Click through the variations | Each shows on the wall, repeating; note seam scores and whether a seam is visible | |
| 7 | **Apply to object**, Ctrl+Z, Ctrl+Y, Ctrl+S, F5 | Material on, off, on; files in `assets\textures` and `assets\materials`; the game shows it | |
| 8 | Close ComfyUI, press **Generate** | "ComfyUI is not running ..." | |
| 9 | Start a generation and press **Cancel** | "Cancelled." at once; ComfyUI stops working | |
| 10 | `UmbraTool texture "old oak planks" --assets assets --count 2` | Progress line, two materials, seam scores | |
