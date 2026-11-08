# ADR 0005: Game systems and tools as source plugins (S2)

- Status: **accepted; implemented** (2026-11-08, `#UMBRA-74`).
- Context: after S1 the user asked for everything to be organized "in the most modular way,
  like plugins". `game/` was one block. The editor and UmbraTool named every AI window and
  command, and the gameplay sounds and AI style file sat in the reference project.

## Decisions (user, 2026-11-08)

| Choice | Decision |
|---|---|
| Kind | Source plugins: folders with a manifest, compiled into one build (no DLLs; C12 unchanged) |
| Scope | Game systems and tools; the core engine (render, world, physics, assets, scene, platform, audio) stays fixed |
| Contents | Code, assets, components and editor windows, so a plugin is added or removed as one unit |

## Design

1. **A plugin is a folder** `plugins/<name>/` with `plugin.json`:
   - `name`, `label`, `description`;
   - `kind`: `game` (part of the game) or `tool` (editor/UmbraTool only, never in the game or the browser build);
   - `requires`, and `requiresAny` (groups where at least one must be on);
   - `modules` (static libraries under `modules/<m>/`) and `links` (what its host code links).
   - Optional folders:
     - `game/`, `editor/` and `tool/`: C++ compiled straight into the game, the editor or UmbraTool;
     - `assets/`;
     - `tests/`;
     - `tools/` (developer tools).
2. **The project chooses.** The `plugins` list in `projects/<name>/project.json` turns plugins
   on. CMake (`cmake/UmbraPlugins.cmake`) adds what they require (saying so) and stops with a
   clear message for an unknown plugin or a missing `requiresAny`. It defines
   `UMBRA_PLUGIN_<NAME>=1` for all code. Programs state the plugins they need
   (`umbra_require_plugins`).
3. **Hosts don't name plugins.** Editor windows register with `UMBRA_EDITOR_PANEL` and reach the
   editor only through `EditorHost` (`apps/editor/EditorPanel.hpp`). UmbraTool commands register
   with `UMBRA_TOOL_COMMAND` (`tools/umbratool/ToolCommand.hpp`). Project components register
   with `UMBRA_COMPONENT` (S1). Plugin host code is compiled into the program, not a library, so
   these self-registrations are never dropped by the linker.
4. **Optional systems are guarded where they meet.** The game is the reference RPG. It needs
   `rpg`, `story`, `rpg-session` and `third-person`, plus one or both combat modes; `learned-ai`
   is optional. The few places that use a combat mode or the learned enemy check
   `UMBRA_PLUGIN_*`: the encounter coordinator, the game session's mode and save, and GameFlow.
   A project with one mode never shows "[Tab] switch", and a save from a two-mode build loads
   into the mode the project has.
5. **Layered assets.** `assets::AssetFolders` holds the project's assets first, then each enabled
   plugin's. `folders / "audio/jump.wav"` returns the first file that exists, so code written for one
   folder kept working. New files are written into the project. Component schemas, the editor's
   asset lists, the web asset pack and the package all use the union, and a project file overrides
   a plugin file of the same path. The game gets only the plugin names and finds
   `plugins/<name>/assets` at run time. The package merges them into `assets/`.

## The plugins

| Plugin | Kind | Modules | Also |
|---|---|---|---|
| `rpg` | game | rpg | editor: RPG content forms |
| `story` | game | story | |
| `turn-combat` | game | turncombat | tool: `fight` |
| `realtime-combat` | game | realtimecombat | |
| `rpg-session` | game | encounter, game | requires rpg, story and turn-combat or realtime-combat |
| `third-person` | game | gameplay | assets: footstep, jump, interact, UI click sounds; tools: make_sounds.py |
| `learned-ai` | game | policy | tool: `policy-check`; tools: Python training + UmbraPolicyEnv |
| `ai-authoring` | tool | questdraft, localai | editor: AI quest draft, AI texture, AI model; tool: `draft`, `texture`, `model`; assets: default `materials/style.json` |
| `web-export` | tool | none | the browser page, service worker, build and check scripts; required by the browser build |

## Consequences

- `game/` is gone; base-game code is under `plugins/*/modules/`. Model inspection (`ModelInfo`,
  `InspectModel`, `FittedCollider`) moved to `engine/authoring` (`ModelTools.hpp`), since the
  editor's "Fit to model" doesn't need AI.
- Exported ComfyUI workflows belong to the project: `projects/<name>/tools/comfyui`.
- Verified on 2026-11-08 with three plugin sets:
  - the reference set (all suites);
  - turn-only with the learned enemy and no AI tools: 35/35, and UmbraTool shows only its commands;
  - real-time only: 31/31.
  The `rpg-session` suites need both modes, so they run with the reference set.
- `engine/authoring` still depends on the RPG plugins (it validates RPG content and saves), so
  the editor and UmbraTool require `rpg`, `story` and `rpg-session`.
