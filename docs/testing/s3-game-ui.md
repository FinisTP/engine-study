# S3 — Gameplay UI framework: verification record

## Record

- Date: 2026-11-08
- Commit: `issue #UMBRA-75` (S3.1–S3.6), ADR 0006
- Tester: Claude (remote session), Linux cloud workspace: GCC 13, Clang 18, ASan/UBSan, Emscripten
  3.1.52, headless Chrome (Playwright). No real GPU: Vulkan is a stub SDK here, and WebGPU on
  SwiftShader loses its device within a second.
- State: **implemented; verified headlessly, including the screens' pixels through the CPU
  rasterizer.** The GPU drawing on Windows (Vulkan) and in Chrome/Edge, gamepad, and the editor
  window are for the user's check (§3).

## What was built

| Task | Where |
|---|---|
| S3.1 Drawing | `core::UiDrawList` (ImGui vertex layout); `engine/ui` Painter (boxes, borders, 9-slice, images, text, clip) and FontAtlas (stb_truetype, `Font.md`); Vulkan: `DiagnosticsOverlay::DrawUi` (background draw list, texture per command); WebGPU: `umbra_gpu_ui` pass |
| S3.2 Screens | `ui/*.ui.json` (8 widget kinds, anchors, stacks, lists/grids), `ui/skin.json` (styles with base and states), bindings, diagnostics with field paths (`UI.md`) |
| S3.3 Input | keyboard, mouse, gamepad (SDL gamepad events) → `UiInput`; focus over last frame's choices, modal stack, `close` / `open:<screen>` |
| S3.4 Replaced UI | HUD, start, pause, death, conversation, fight screens in the `rpg-ui` plugin; `GameFlow::UiData` / `HandleUi`; ImGui menus, `gameplay::PauseMenu` and the browser's HTML HUD removed; `Module.uiState` for checks |
| S3.5 New screens | inventory/equipment (equip, unequip), skills (unlock), quest log; I/K/J or the pause screen; they pause the game |
| S3.6 Editor and tools | editor **Game UI** window (preview over the scene view with `ui/sample.json`, widget tree, field editing, checked edits, undo, save to project); `UmbraTool ui-preview` (CPU rasterizer → PNG) |

## 1. Automated checks

| Check | Result |
|---|---|
| Linux GCC / Clang / GCC ASan+UBSan, reference project | **47/47 / 47/47 / 47/47** |
| T78 plugin sets with `rpg-ui` (turn-only + learned AI; real-time only) | **37/37, 33/33**; without `rpg-ui` the configure stops: "The game needs the plugin 'rpg-ui'" |
| `Umbra.Ui` (T79): atlas, bad files with field paths, focus/confirm/back, every reference screen draws with the sample data, inventory and skill screens send `equip`/`unequip`/`unlock` (not for disabled entries), rasterizer blend and clip | pass (also under ASan) |
| `Umbra.Tool.UiPreview` | pass; 9 screen combinations rendered and looked at (below) |
| Headless suites in WebAssembly (Node), now with `Umbra.Ui` | **14/14** (also fixed: plugin test data define and tool tests missing from the browser test build since S2) |
| Browser build (`check-web.py`) | **13/13**: hero on the HUD screen (`Module.uiState`), Tab → real time, Esc opens and closes the pause screen, I opens the inventory, saves, offline, damaged assets, no WebGPU; no WebGPU validation errors in the console |

`UmbraTool ui-preview` pictures checked by eye: HUD (bars, quest tracker, enemy boxes, prompt,
key help), pause, inventory, skills, quests, conversation, fight, start and death screens. Fixed
from them: a gold frame around the whole HUD (root used the panel style), the enemy tag joined
to its name in the sample data, the `[E]` prompt showing during conversations.

## 2. Known limits

- Text is bitmap glyphs at the skin's pixel sizes (no SDF, no right-to-left or complex shaping).
- The editor preview has no input; screen edits have their own Undo, not Ctrl+Z.
- The game does not reload screen files while running (the editor's Play starts it fresh).

## 3. For the user (Windows)

| # | Do | Expect |
|---|---|---|
| 1 | Reconfigure and build; start the game | HUD drawn in the game's own fonts (Poppins/Lora), no ImGui menu boxes |
| 2 | Esc; move with W/S and the mouse; Resume | Pause screen dims the scene, the game and its sounds stop; the mouse is free; resume recaptures it |
| 3 | I, K, J while exploring | Inventory (equip the sword, take it off), skills (unlock one with points), quests |
| 4 | Talk to the elder; fight in turn mode; die once | Conversation, fight and death screens; mouse clicks work on their entries |
| 5 | A gamepad: d-pad, A, B, Start | Focus moves, A chooses, B goes back, Start pauses |
| 6 | 150 % Windows scaling | Mouse hits the right entries; text still sharp |
| 7 | Editor: View → Game UI; pick `inventory`; change a text and an anchor; Save; Play | Preview updates over the scene view; a bad value shows in red; the game shows the change (file in `projects/reference/assets/ui/`) |
| 8 | `scripts\serve-web.bat`, Chrome/Edge | Same screens as on PC; I/K/J and Esc work |
