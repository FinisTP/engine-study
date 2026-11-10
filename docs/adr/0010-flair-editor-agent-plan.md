# ADR 0010: RPG flair, editor and agent plan (S6-S17)

- Status: **accepted; planned** (2026-11-08). Nothing implemented yet.
- Context: every numbered roadmap phase and S1-S5 are implemented. The user asked for more
  planning to improve the editor, the AI agent's convenience and the game's RPG flair, chose
  every proposed item, added a cutscene system, an animation/SFX event system, rigging, custom
  shaders and post-processing, and asked for **an example of each improvement**. RPG flair
  comes first (user choice).

## What exists today (the gaps)

| Area | Today |
|---|---|
| Rendering | One fixed directional light (Lambert, 25% ambient) in `Scene.hlsl`; no shadows, fog, sky, point lights, HDR or post-processing; materials pick a texture only |
| Animation | Procedural walk only; glTF skins and clips are not imported; AI models (TRELLIS.2, Meshy) arrive unrigged |
| Feel | No particles, damage numbers, hit-stop or camera shake; sounds are played directly by game code |
| RPG content | Statuses, skills, items, actors, one quest, one conversation; no gold, shops, loot, chests, doors or dialogue choices with consequences; no cutscenes |
| Editor | Scene editing with gizmo, RPG forms as lists, AI windows; no prefabs, thumbnails, drag-and-drop, multi-select, play inside the editor or graph editors |
| Agent | One request at a time, no memory, no idea of the selection or the cursor, cannot see the scene |

## Plan

Each phase ships its **example** in the reference project (or a sample), listed with it. Every
rendering feature works on Vulkan (PC, Android) and WebGPU (browser), or the phase says what the
browser shows instead. The performance target stays (ROADMAP O18: 16.67 ms average on the user's
PC at 1920x1080); phones get a lower quality setting.

### Wave 1: RPG flair

| Phase | What | Example |
|---|---|---|
| **S6 Lighting & atmosphere** | Sun and sky light (hemisphere ambient), distance/height fog, sky gradient, up to 16 point lights per frame (torches, fires, crystals), one sun shadow map with soft edges (PCF), day/night cycle component that moves the sun and blends sky, fog and ambient; light components in scenes | The settlement at dusk with torches on the houses; the dungeon lit only by torches; a day/night cycle on the settlement |
| **S7 Post-processing & custom shaders** | HDR render target, tonemapping, bloom, color grading (3D LUT), vignette; a per-scene post stack component. Custom material shaders: a material may name a small HLSL surface function inserted into the engine's template (inputs: UVs, normal, time, world position); compiled with the engine's shaders; the browser gets them through a SPIR-V -> WGSL step | A water surface in the well (scrolling ripples), a glowing crystal (emissive + bloom), a cold blue grade in the dungeon and a warm one at sunset |
| **S8 Skeletal animation & rigging** | glTF skins (joints, weights) and clips imported; GPU skinning; clips sampled with interpolation and crossfaded; an animation graph per model (states idle/walk/run/attack/hit/die, conditions from the game) in a data file. AI: rig a generated model through an exported ComfyUI workflow (UniRig), make clips from text (HY-Motion) and retarget them, like P8D | Hero, wolf and bandit animated (idle, walk, attack, hit, death); an AI-made villager rigged and given a "wave" clip from a text prompt |
| **S9 Animation events, SFX & VFX** | Events on clip frames (footstep, hit, cast) that play sounds, spawn effects and time the damage; a particle system (`*.vfx.json`: rate, lifetime, velocity, gravity, colour and size over life, texture, additive/alpha) with GPU billboards; game feel: damage numbers, hit-stop, camera shake, screen flash; skills, items and statuses name their effects in the RPG content | Sword swings with footstep and hit sounds on the right frames; a fireball skill; fire on the torches; poison bubbles over a poisoned actor; numbers and a short freeze on a heavy blow |
| **S10 RPG systems** | Gold, merchants and shops (buy/sell screens); loot tables and drops; chests, doors and levers (locks and keys); dialogue choices with conditions and consequences (flags, items, gold, reputation); a quest with two outcomes | Merchant Tobin in the settlement sells potions; the bandit leader drops the key to a locked chest in the dungeon; the elder asks for help: accept (reward) or refuse (she remembers it) |
| **S18 Party & characters** (added 2026-11-10) | Up to four party members: companions recruited through dialogue consequences (S10), who follow the leader while exploring; every member has their own stats, level, equipment and skill tree; shared inventory and gold. Turn fights: each member acts on their turn, chosen by the player. Real-time fights: the controlled member is switched with a key, and the others use conventional companion AI. A character screen (stats, equipment, skills, swap leader) and a party bar in the HUD; party in saves | Recruit Mara the hunter (bow, ranged skills) in the settlement after the elder's quest; a turn fight with hero and Mara against the bandits; give Mara the sword in the character screen |
| **S11 Cutscenes** | A timeline file (`*.cutscene.json`) with tracks: camera (keyframes, look-at), actor moves and clips, dialogue lines, sounds and music, fades, waits; started by a trigger zone or a quest step; skippable; saved as seen | An intro: the camera flies over the settlement to the elder, who speaks; a short reveal before the bandit leader's fight |

### Wave 2: Editor

| Phase | What | Example |
|---|---|---|
| **S19 Resource previewers** (added 2026-11-10) | A preview for every asset that can be clicked in the inspector or the asset browser, except code (which opens Visual Studio): sounds and music (play/stop/loop, position, volume, waveform, length, format), images (zoom, checkerboard alpha, size, mip levels, tiled view), models (turntable rendered by the engine, meshes/triangles/materials, clips after S8), materials (lit sphere and cube, shader), UI screens (the existing Game UI preview), RPG content (item/skill/character cards). One preview window that follows the last asset clicked, so it doubles as the thumbnail renderer S12 needs | Click `sounds/torch.wav` and hear it; click `models/hero.glb` and turn it; click `materials/well_water.material.json` and see the water moving on a sphere |
| **S20 Editor settings** (added 2026-11-10) | A Settings window saved per user (`editor-settings.json`, next to `umbra-editor.ini`). Preferences: colour scheme (dark, light, classic, plus an accent colour), UI scale and font size, hotkeys rebinding for every editor command with conflict warnings. Diagnostics: the diagnostics overlay on/off (editor and game), frame-time graph, Vulkan validation on/off (takes effect on restart), log level | Switch to the light scheme, bind "Play" to F6 instead of F5, hide the diagnostics overlay; all of it kept after a restart |
| **S12 Prefabs & drag-and-drop** | `*.prefab.json` (an entity tree with components); scene entities that use a prefab keep only their overrides; the asset browser shows thumbnails (rendered by the engine); drag a model or prefab into the view (lands on the ground); multi-select, Ctrl+D duplicate, grid and angle snapping | A torch prefab placed ten times along the road; changing the prefab's light colour updates all ten; a market stall prefab |
| **S13 Play in editor** | The game runs inside the editor's view (Play / Pause / Stop, "play from here"); the scene returns to its edited state on Stop; entities can be inspected while playing | Walk around the settlement, fight the wolf, press Stop: everything is as it was |
| **S14 Graph editors** | Conversations and quests as node graphs (choices, conditions and consequences from S10), the cutscene timeline (S11) and the animation graph (S8) as visual editors; the files stay readable JSON | The elder's conversation as a graph; the merchant's conversation built from nothing in the graph |

### Wave 3: Agent

| Phase | What | Example |
|---|---|---|
| **S15 Agent chat & vision** | The Agent window keeps the conversation (follow-ups: "make it bigger"); the selected entity and the point under the cursor are in the context ("this", "here"); a `look_at_scene` tool renders the view and a local vision model (e.g. `qwen3-vl:8b`) checks the result; tools for everything added in S6-S14 (lights, post, prefabs, effects, shops, choices, cutscenes) | "Put a torch here" (at the cursor), then "make it warmer and brighter"; "does the market look crowded? spread it out" with a rendered check |
| **S16 Recipes & scatter** | Recipe files (a prompt with parameters) as one-click buttons; a scatter tool (N prefabs or models in a circle, box or along a path, random turn and scale, on the ground, not overlapping) | Recipes "Market square", "Forest edge", "Dungeon room"; "scatter 20 trees around the gate" |
| **S17 Code agents** | A `create_component` scaffold (schema file, C++ stub, registration, test); the engine's format documents served as MCP resources and common tasks as MCP prompts; `UmbraTool agent-eval` runs a task set against a model (dry runs checked against expected results) and scores it | Claude Code adds a "bob" component (floating up and down) through MCP, builds and tests it; an eval report comparing two local models |

## Order and dependencies

```text
S6 lighting -> S7 post + shaders            (HDR needs the lit scene)
S8 skeletal -> S9 events/SFX/VFX -> S11 cutscenes (cutscenes move animated actors)
S10 RPG systems (independent; choices feed S14 and S11) -> S18 party (companions are recruited by dialogue consequences)
S19 previewers -> S12 prefabs (the preview renderer makes the asset thumbnails); S20 settings (independent)
S12 prefabs -> S13 play in editor -> S14 graph editors
S15 agent chat/vision -> S16 recipes/scatter (scatter uses prefabs, S12) -> S17 code agents
```

The order is S6, S7, S8, S9, S10, S18, S11, S19, S20, S12, S13, S14, S15, S16, S17 (S18-S20 added
from the user's S6/S7 review on 2026-11-10; the numbers are kept so earlier references stay valid). Agent tools for new
systems are added in S15 and listed there, so earlier phases stay small.

## Decisions to confirm when each phase starts

| Phase | Question | Default |
|---|---|---|
| S6 | Shadows: one shadow map, or cascades | One 2048 map fitted to the area around the player; cascades only if distant shadows look poor |
| S7 | Custom shaders in the browser | SPIR-V -> WGSL at build time with a converter (new build-time dependency, reviewed then) |
| S8 | AI rigging and motion models | UniRig and HY-Motion through ComfyUI custom nodes on the creator's PC; licenses shown like other model licenses and checked before shipping |
| S14 | Graph editor UI | `imgui-node-editor` (MIT; new dependency, reviewed then) |
| S15 | Vision model | `qwen3-vl:8b` on 8 GB GPUs, measured on the S15 example requests |
| S18 | Party size and real-time control | Up to 4; in real-time fights the player controls one member (switch with Tab) and the others use companion AI |
| S18 | The learned enemy policy (P8A) with more heroes | The observation stays v1 (it sees the targeted hero); a party-aware policy is a later retraining |
| S19 | Model previews | Rendered off-screen by the editor's Vulkan renderer into an ImGui image (no second window) |
| S20 | Where settings live | Per user, beside `umbra-editor.ini`; nothing in the project (a team shares the project, not preferences) |

Answered (user, 2026-11-09): S6 uses the default (one 2048 map). The time of day is saved in save
games, the settlement runs a 12-minute day that starts at dusk, and nights are moonlit and readable.
S7 converts SPIR-V to WGSL at build time. On mobile, post-processing is reduced: tonemapping, grading
and vignette stay on, and bloom runs at half resolution.

Answered (user, 2026-11-10): S8 uses the default for AI (UniRig and HY-Motion through a local
ComfyUI, tested with a stand-in server). The example characters are rigged and animated with Meshy
(the hero and bandit: idle, walk, run, attack, hit, death), and the wolf is a free CC0 animated model
(Quaternius). Meshy could not rig the elder (her robe hides the legs), so she stays unrigged; the
"AI villager waving" example needs the user's ComfyUI run (`testing/s8-skeletal-animation.md` §4).

## Consequences

- Twelve phases, each with tests and its example; Windows checks per phase as before.
- Rendering phases change both renderers (Vulkan and WebGPU) and the Android quality setting.
- New AI models (rigging, motion, vision) stay on the creator's PC, never in the game.
