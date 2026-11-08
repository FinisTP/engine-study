# ADR 0006: The game draws its own UI from screen files (S3)

- Status: **accepted; implemented** (2026-11-08).
- Context: the game's HUD and menus were Dear ImGui calls on PC and HTML on the web page, two
  copies that looked different, could not show pictures or be restyled, and were written in C++.
  The user asked for a gameplay UI framework with inventory, skill tree and quest log screens.

## Decisions (user, 2026-11-08)

| Choice | Decision |
|---|---|
| Authoring | Screens are data (`assets/ui/*.ui.json`) with a live preview in the editor; C++ only for new widget kinds and the game's data and actions |
| Drawing | The engine draws the UI itself: the same look on PC and in the browser. ImGui stays for debug windows and the editor |
| Scope | Replace all current game UI and add inventory/equipment, skill tree and quest log |

## Design

1. **`engine/ui` is a core module** and headless: skin, fonts, screen files, layout, bindings,
   focus. Its output is triangles in `core::DrawList::ui`; it never touches a window, a GPU or
   ImGui, so the same code runs in tests, UmbraTool, the editor, the game and the browser.
2. **Renderers only draw triangles.** Vulkan copies them into the ImGui background draw list
   (same vertex layout) and binds the right texture per command; WebGPU has a second small
   pass with one textured, alpha-blended pipeline.
3. **The game gives one JSON object** per frame (`GameFlow::UiData`) and gets back actions
   (`GameFlow::HandleUi`). Screens bind to the data by path (`{hero.health}`), so a screen file
   can change without C++ changes as long as the data has what it needs.
4. **The RPG screens live in the `rpg-ui` plugin**, with the skin and two OFL fonts. A project
   overrides any file by putting one at the same path in its own assets.
5. **Previews without a GPU:** a CPU rasterizer draws the same triangles to a PNG
   (`UmbraTool ui-preview`), so screens can be checked headlessly, by tests and by AI agents.

## Consequences

- The browser page no longer draws a HUD; it shows asset warnings and timings, and gets the open
  screens and hero numbers as `Module.uiState` for its checks.
- `gameplay::PauseMenu` was removed; the pause screen is a file.
- Text is bitmap glyphs at fixed pixel sizes (no SDF): crisp at the size given in the skin, not
  meant for scaling animations. Right-to-left and complex scripts are not shaped.
- Screen edits in the editor have their own undo, separate from the document undo.
