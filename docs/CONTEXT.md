# Umbra — project context and confirmed decisions

**Version:** 1.2  
**Requirements updated:** 2026-11-01, through user answers 1–23  
**Implementation evidence:** status document dated 2026-09-02; partial repository check (B0) on 2026-11-01  
**Companions:** [Working instructions](../CLAUDE.md) · [Implementation roadmap](ROADMAP.md)

## 1. Purpose and authority

This file preserves the project brief so a coding agent can work without the previous conversation. It records requirements and documentary status; it is not proof that new code exists. The roadmap supplies the implementation plan, technical defaults, milestone gates, and tests.

Within project documents, distinguish **intent** from **evidence**. The user's latest explicit decisions govern product intent. Source code, tests, and recorded runs establish implementation evidence. A discrepancy between them is a gap to report, not permission to silently replace either. These files do not override tool permissions or higher-priority security policies.

Source documents used:

| Source | Role and limits |
|---|---|
| `docs/custom-3d-game-engine-roadmap.md` | Original learning-first architecture and historical roadmap. Contains older status text as well as a later Phase 2 completion note. Its incompatible future scope is superseded below. |
| `docs/current-engine-status.md` | Implementation report dated 2026-09-02. Records Foundation and Rendering complete and Assets and World next. |
| `docs/testing/phase1-smoke-test.md`, `phase2-rendering-smoke-test.md` | Manual verification records. Phase 2 is marked passed; Phase 1 is still "Partial" (checklist unticked). |
| `docs/testing/b0-baseline-reconciliation.md` | Partial source inspection and Linux test run, 2026-11-01. |
| `umbra-rpg-roadmap-baseline-2026-11-01.md` | Prior proposed roadmap based on answers 1–20, including T01–T38. Not in the repository. This pack updates that plan, resolves its D1–D3 questions, and retains their test identifiers. |
| User answers 1–23 in the planning conversation | Confirmed requirements, overrides, and latest decisions. Reproduced as a requirement ledger below. |

Inspect the checkout before following historical paths or commands; this file can go stale.

## 2. Product identity

Umbra is a learning-first, modular C++20 full-3D RPG engine and PC creation tool. Its first meaningful product is a small complete single-player RPG. It should be understandable to an individual creator and to AI coding agents, quick to iterate, and extensible without accumulating unrelated framework code.

The product combines **data-authored RPG content**, **C++ system extension**, **AI-assisted quest/dialogue authoring**, and a **locally executed learned turn-based enemy policy**. It is not a miniature feature-for-feature Unreal replacement and does not depend on cloud AI for gameplay.

Learning remains part of the original intent. Prefer a focused experiment when it teaches a needed engine concept; do not let a research branch become an accidental prerequisite for the reference game.

## 3. Confirmed requirement ledger

IDs C01–C15 retain the previous baseline's grouping. C16–C18 resolve the final three answers. All entries are requirements, except where explicitly labeled future intent or unspecified.

| ID | Confirmed requirement | Source answers |
|---|---|---|
| C01 | Release sequence: PC first → playable web export → mobile → other platforms. Windows x64 is the existing first PC implementation. | 1, 18; source status |
| C02 | Editor remains on Windows/PC. No browser editor is requested. | 18 |
| C03 | Full 3D and single-player first. 2D/2.5D may come later; multiplayer and extensive online architecture can be skipped now. | 2, 3, concluding note after 10 |
| C04 | Reference game: one settlement, one dungeon, one player character, one NPC, one enemy, one conversation, one skill tree, one turn-based combat system, one real-time fighting/exploration system, one quest. | 11 |
| C05 | Systems: movement, quest, save/load, turn-based combat, real-time combat, stats, conversation, inventory/equipment, skills, NPC/enemy AI. Preserve these responsibilities even if code organization groups them differently. | 11 |
| C06 | Hybrid authoring. Systems are extensible in C++; supported content can be added using data only. A separate scripting framework may be considered later. | 7, 12 |
| C07 | Initial AI features: AI-assisted quest/dialogue authoring and runtime learned enemy combat behavior. Developers supply usable behaviors/skills and relevant observations. Local execution/training is preferred; cloud is an optional backup, not a dependency. | 4, 13 |
| C08 | Developers train before shipping. Players execute the resulting model. AI tooling should be approachable without extensive ML expertise. | 4, 14 |
| C09 | Gameplay needs no network after installation. Save/player data is local initially. | 5, 10, 15 |
| C10 | Future intent: with player agreement, contributions may improve enemies across players. The term “local enemy knowledge,” delivery design, and release phase were not specified. This is not authorization to build a cloud service now. | 15 |
| C11 | Prioritize reliable content reload, small incremental builds, and checkpoint-based play restart. Seamless native code replacement is optional. | 16 |
| C12 | Modding and public plugin support are out of initial scope. This supersedes the earlier yes to mods. | 17 supersedes 6 |
| C13 | Target 60 FPS on a mid-range PC. Exact CPU/GPU/RAM/VRAM, resolution, scene, settings, and budgets are intentionally deferred. | 9, 19 |
| C14 | Model work runs separately from rendering. Training experiments remain independent of engine features required for the RPG. | 19, 20 |
| C15 | No blanket third-party-library restriction. This does not waive compatibility, maintenance, security, licensing, or distribution review. | 8 |
| C16 | Turn-based and real-time combat are two independent modes, integrable when needed. Both appear in the reference vertical slice; other games may use either alone. | 21 |
| C17 | First learned enemy policy targets turn-based combat. A learned real-time policy is not an initial requirement. | 22 |
| C18 | Increasing aggression when player HP is lower is a learned preference, not a mandatory rule or guaranteed outcome. | 23 |
| C19 | Product direction (2026-11-10, ADR 0011): ship and sell an open-ended storytelling simulation RPG. An authored main plot always wins in the end; the simulated world is the core gameplay and the hook. Scale: one small town with about seven characters (solo developer). The theme is open and need not be fantasy (a modern everyday story is possible). | Decision of 2026-11-10 |
| C20 | A built-in AI model runs in the shipped game and makes the world proliferate. It is required for such a game and ships with it, so play stays fully offline. This supersedes "generation is authoring only, no mandatory runtime LLM" (C07/C08 reading) for games that use the living-town plugin. | Decision of 2026-11-10; supersedes part of 13, 14 |
| C21 | Every playthrough must produce different outcomes, and the developer manages those outcomes so they fit the main story. | Decision of 2026-11-10 |

### Required system interpretation

“One” defines the minimum reference content, not the universal capacity of the engine. Whether “one enemy” means one placed instance or one reusable archetype is not explicitly settled. A reversible planning default is one enemy definition reusable in separate test/reference encounters; do not expand this into multiple enemy families without need.

The reference game must demonstrate both modes, but the engine must not force every game to link/instantiate both controllers. Sharing definitions and authoritative state is encouraged; coupling turn scheduling to real-time scheduling is not.

The player-facing learned enemy is a compact policy choosing allowed actions. Since C20 a living-town game also runs a bundled language model, but only as a proposer: it chooses among legal intents, proposes authored kinds of events and words lines; the simulation validates and commits. It still does not generate executable logic or learn on the player's machine.

## 4. Documented implementation baseline

The status report says the user completed Phase 2 build/runtime/validation/resize/shutdown/RenderDoc checks on 2026-09-02, recorded in `docs/testing/phase2-rendering-smoke-test.md`. The Phase 1 interactive record (`docs/testing/phase1-smoke-test.md`) is still marked "Partial" with no items ticked, even though the status doc calls Phase 1 complete.

A partial check on 2026-11-01 confirmed the source and docs exist as described. Phase 3 was then implemented the same day; all 13 CTest entries pass on Linux (GCC, Clang, GCC ASan+UBSan). No Windows build, renderer run or GPU check has been repeated since 2026-09-02.

### Existing stack reported by the source

| Area | Documented selection |
|---|---|
| Language/toolchain | C++20; MSVC x64 primary, clang-cl verification |
| Build/dependencies | CMake, Ninja Multi-Config, manifest-mode vcpkg with pinned registry baseline |
| Platform | SDL3 |
| Diagnostics/tests | Engine-owned logging backed by spdlog; Catch2/CTest; warning policies, ASan configuration, CI |
| Graphics | Vulkan through Volk; GPU allocation through VMA |
| Shaders | HLSL compiled to SPIR-V by DXC, checked by `spirv-val` |
| Scene interchange | Controlled glTF 2.0 import using TinyGLTF |
| Diagnostics UI | Dear ImGui overlay with an Umbra-owned Vulkan renderer |

Versions listed in the older roadmap are historical observations, not instructions to upgrade or proof of current dependencies. Read manifests and presets before changing them.

### Reported completed foundation

Modular targets, SDL window/event/input/lifecycle handling, monotonic timing, fixed-step accumulator, diagnostics and failure handling, compiler policies, tests, sanitizer configuration, CI, and ownership/shutdown behavior.

### Reported completed renderer

Instance/device/surface/swapchain ownership; VMA buffers/images and staging upload; indexed rendering with camera, transforms, depth; two frames in flight; synchronization and per-frame uniforms; stable engine-facing mesh/texture handles; basic textured Lambert shading; diagnostics overlay; resize/minimize/restore recovery and RenderDoc inspection.

### Explicit limitations

The importer handles the first primitive and one base-color texture of a controlled glTF asset. Textures have one mip level. Initialization uploads wait for the graphics queue. Existing stable resource handles are **not proof that general generational resource management is implemented**.

Phase 3 (2026-11-01) added the general world, glTF hierarchy import, generational resource handles, derived asset cache and transactional reload, all still pending Windows/GPU verification. Phase 4 (2026-11-06) added the Jolt physics adapter, a third-person player, procedural walk animation, WAV audio, interactables, a pause menu, portals between two areas and development checkpoints, also pending Windows/GPU/interactive verification. Phase 5 (2026-11-07) added headless RPG rules (content, stats, inventory, skill tree, shared resolver) and independent turn and real-time combat controllers with a coordinator. Phase 6 (2026-11-07) added conversations, quests, XP/levels, saves and the playable reference game (roaming enemies, arena fights, death menu), pending Windows/interactive verification. Phase 7 (2026-11-07) added the creator workflow: authoring documents with undo, a command-line tool, a separate editor, live content reload and packaging, pending Windows/interactive verification. P8A (2026-11-07) added the learned turn-based enemy. P8B (2026-11-07) added AI-assisted quest authoring through a local Ollama model, P8C (2026-11-07, user request) AI texture generation through a local ComfyUI plus materials with world-space texturing and mipmaps, and P8D (2026-11-07, user request) AI 3D props through ComfyUI's TRELLIS.2 (all three verified on Windows with real models by the user on 2026-11-08; P8D limitations fixed in `#UMBRA-62`). The source does not establish PBR/HDR/shadows/IBL, a skeletal animation system, or a C# host. Do not infer completion from roadmap language or empty directories.

### Observed target graph (2026-11-08, after S1; base-game modules live in `game/`, the rest in `engine/`)

```text
UmbraEditor (apps/editor: Editor, Components, CodePanel, UiPanel, RpgForms, AiDraft, AiTexture, AiModel, AiSound, AiAnimation; compiles apps/umbra/ModelLibrary) → Authoring, LocalAi, Render, Platform, Scene, imgui[sdl3], imguizmo
UmbraTool (tools/umbratool) → Authoring, Ui, LocalAi, TurnCombat, Policy (validate, inspect, schema, fight, policy-check, draft, texture, model, sound, rig, motion, package-check, web-pack, ui-preview)
Umbra::LocalAi → Authoring, Assets, Audio, httplib, libvorbis (local-only Ollama bridge for quest drafts, ComfyUI bridge for textures, image-to-3D, music and sound effects; never linked by the game)
Umbra::Authoring → Scene, Rpg, Game, Assets, nlohmann-json (validated content documents, undo, quest-draft plans + play-through; never linked by the game)
UmbraPolicyEnv (tools/policy, shared library for Python training; never linked by the game) → Policy
Umbra (apps/umbra: Main, ModelLibrary, SceneRuntime, GameFlow, GameComponents + the project's engine/ and game/ C++)
  ├─ Umbra::Policy → TurnCombat, Threads             (learned turn enemy: contract, environment, MLP, decider)
  ├─ Umbra::Game → Story, Encounter, nlohmann-json   (saves, GameSession: story + fights + saves)
  │     Umbra::Story → Rpg                            (quest log, conversations)
  │     Umbra::Encounter → TurnCombat, RealtimeCombat (combat coordinator)
  │     Umbra::TurnCombat → Rpg      Umbra::RealtimeCombat → Rpg   (independent controllers)
  │     Umbra::Rpg → Core, nlohmann-json  (content, characters, inventory, skills, shared resolver)
  ├─ Umbra::Gameplay → World, Physics                (player, roamers, physics bridge, interaction, areas)
  ├─ Umbra::Ui      → Core, Assets, nlohmann-json    (game screens: skin, fonts, layout, bindings, focus; GameUi in apps/umbra)
  ├─ Umbra::Physics → Core; Jolt private             (bodies, raycasts, character)
  ├─ Umbra::Audio   → Core, Assets                   (WAV and Ogg (stb_vorbis), mixer with streamed music, sound bank)
  ├─ Umbra::Scene   → World, Assets, nlohmann-json   (scene files, components and their schemas, model instancing, checkpoints)
  ├─ Umbra::Assets  → Core, tinygltf, nlohmann-json  (glTF import, .umodel cache, file watcher, materials, image tools)
  ├─ Umbra::World   → Core                           (entities, hierarchy, renderables)
  ├─ Umbra::Render  → Core, Diagnostics, Platform, SDL3, Volk, VMA, glm, imgui; HLSL via DXC
  ├─ Umbra::Platform → SDL3, Audio                  (window, events, input, audio device)
  ├─ Umbra::Diagnostics → spdlog
  └─ Umbra::Core    → glm                            (SlotMap, DrawList, MeshData, RetireQueue, Diagnostic, clocks)
UmbraSceneCheck → Scene, Assets                      (headless validator CLI)
Tests: Umbra.Core, .Platform, .Diagnostics.*, .Render, .World, .Assets, .Scene, .Physics, .Ui,
       .Gameplay, .Audio, .Rpg, .TurnCombat, .RealtimeCombat, .Encounter, .Story, .Game,
       .SceneCheck.Sample/.Settlement/.Dungeon/.Arena/.Broken, .HeadlessDependencies,
       .TurnOnlyDependencies, .RealtimeOnlyDependencies, .Authoring, .Tool.*, .GameExcludesEditor, .Policy, .PolicyDependencies,
       .LocalAi, .EngineOffline
```

Core, World, Assets, Scene, Physics, Gameplay, Audio, Rpg, TurnCombat, RealtimeCombat, Encounter, Story, Game, Authoring, Policy, UmbraSceneCheck and UmbraTool must stay headless (Policy also turn-only, no ML runtime or Python); the game must not link the editor, Authoring or LocalAi (`Umbra.GameExcludesEditor`); only LocalAi may use the network, and only to this computer (`Umbra.EngineOffline`); each combat controller must build without the other; `Umbra.HeadlessDependencies` enforces it. App path: `/apps/umbra`, executable `Umbra`. Editor app: `/apps/editor`, executable `UmbraEditor`. Content: `/projects/reference/assets`. Still-empty folders: `/apps/game`, `/engine/script`, `/engine/editor` (`/samples` now holds the W0 web sample). Do not rename the existing app to `/apps/game` because an illustrative tree used that name.

Commit messages follow `issue #UMBRA-<n> <summary>`. Phase 3 is `#UMBRA-13` … `#UMBRA-20`; Phase 4 is `#UMBRA-21` … `#UMBRA-25`; Phase 5 is `#UMBRA-26` … `#UMBRA-33`; Phase 6 is `#UMBRA-37` … `#UMBRA-43`; Phase 7 is `#UMBRA-44` … `#UMBRA-49` (editor fixes `#UMBRA-50`/`#UMBRA-51`); Meshy models `#UMBRA-52`; P8A `#UMBRA-53`/`#UMBRA-54`; P8B `#UMBRA-55` (fixes `#UMBRA-56`/`#UMBRA-57`); P8C plan `#UMBRA-58`, P8C `#UMBRA-59`; script fix `#UMBRA-60`; P8D `#UMBRA-61`, fixes `#UMBRA-62`/`#UMBRA-63`; P9 `#UMBRA-64`, `#UMBRA-65`; P10 `#UMBRA-66` … `#UMBRA-68`; web pack `#UMBRA-70`; editor grid `#UMBRA-69`; S1 components `#UMBRA-71`, layers `#UMBRA-72`; S2 plugins `#UMBRA-74`; S3 game UI `#UMBRA-75` (MSVC fix `#UMBRA-76`); S4 sound, music, menus `#UMBRA-77`; S5 plan `#UMBRA-78`; P11 Android `#UMBRA-79`; S5 agent prompt `#UMBRA-80`; S6-S17 plan `#UMBRA-81`; S6 lighting `#UMBRA-82`; S7 post and custom shaders `#UMBRA-83`; S8 skeletal animation and AI rigging, crash dumps, plan for S18-S20 `#UMBRA-84`; living-town plan (ADR 0011) `#UMBRA-85`; S9 animation events, SFX and VFX `#UMBRA-86`.

**Direction (ADR 0011, user 2026-11-10):** S9 is implemented (`#UMBRA-86`); next the living town, S21-S25: world tick and residents, memory and chronicle, the built-in model, the storyteller with a developer-authored outcome plan, and an example town; S10 and S18 are re-scoped into them; a living-town game is PC only for now. **Earlier plan (ADR 0010, user 2026-11-08):** S6-S17, RPG flair first (lighting, post-processing and custom shaders, skeletal animation with AI rigging, animation events/SFX/VFX, RPG systems, cutscenes), then the editor (prefabs and drag-and-drop, play in editor, graph editors), then the agent (chat with context and vision, recipes and scatter, code-agent support); each phase ships an example in the reference project. **Current milestone:** S9, animation events, SFX & VFX (`#UMBRA-86`), is implemented: clip events with a sound and an effect that fire once per crossing, a CPU particle system from `vfx/*.vfx.json` drawn as camera-facing squares on Vulkan and WebGPU (`engine/core/include/Particles.md`), the `vfx` component, blows that land on the attacker's `hit` event with damage numbers, hit-stop, shake and a red flash (`apps/umbra/GameFeel.md`), and an `effect` on skills, items and statuses; example: torch fire, timed footsteps with dust, a fireball skill, poison bubbles. Verified headlessly (T89) and in software-rendered pictures (`docs/testing/s9-events-sfx-vfx.md`); the user's check with a GPU and sound is open. S8, skeletal animation & rigging (`#UMBRA-84`, user decisions 2026-11-10: Meshy rigs and clips for the hero and bandit, a CC0 animated wolf, AI rigging and motion through a local ComfyUI), is implemented: glTF skins and clips are imported and cached, skinned on the GPU (Vulkan and WebGPU: a per-frame joint buffer, skinned scene, shadow and surface-shader pipelines), and driven by animation graphs (`animations/*.anim.json`: the first matching state plays, crossfades, triggers for one-shots, a speed-driven rate; `engine/core/include/Skeleton.md`, `engine/scene/include/AnimGraph.md`) through the `animation` component and the RPG actor `animation` field; in the game the hero, bandits and wolves idle, walk, run, attack, flinch and fall; the editor plays idle in its view and has an AI animation window that rigs a model (UniRig) or makes a clip from text (HY-Motion) through the creator's exported ComfyUI workflows (`UmbraTool rig`/`motion`; FBX converted with Blender; `plugins/ai-authoring/modules/localai/include/AnimationGen.md`). Verified headlessly (T87, T88, stand-in ComfyUI) and in software-rendered Vulkan and WebGPU pictures (`docs/testing/s8-skeletal-animation.md`); the user's GPU check and a real UniRig/HY-Motion run are open. S7, post-processing & custom shaders (`#UMBRA-83`, user decisions 2026-11-09: WGSL converted from SPIR-V at build time with naga, reduced post on phones), is implemented: the scene is drawn into an HDR picture, then bloom (6 levels; 4 from quarter size on Android), ACES tonemapping, a baked 16³ color table (an optional 256×16 LUT image plus temperature, saturation and contrast) and a vignette, set per scene by the `post` component (`engine/core/include/Post.md`); materials may name a custom surface shader (`assets/shaders/<name>.surface.hlsl`, one HLSL function inserted into the scene shader; a Vulkan pipeline per shader; DXC + naga make the browser's WGSL; errors name the shader's own line; `engine/render/shaders/CustomShaders.md`). Example: warm settlement evening with rippling well water, cold dungeon with a LUT and a pulsing crystal. Verified headlessly (T85, T86) and in software-rendered Vulkan and WebGPU pictures (`docs/testing/s7-post-and-shaders.md`), and by the user's Windows review of S6 and S7 (2026-11-09). That review added S18 party & characters, S19 resource previewers and S20 editor settings to the plan (ADR 0010; order S8, S9, S10, S18, S11, S19, S20, S12-S17) and reported a crash on undo in the Game UI window (open; crash dumps added). S6, lighting & atmosphere (`#UMBRA-82`, user decisions 2026-11-09: the time of day is saved, a 12-minute day starting at dusk, moonlit readable nights), is implemented: linear light with sRGB textures and screen, sky and ground ambient, sun or moon with a 2048² shadow map, up to 16 flickering point lights (`light` component), `emissive` surfaces, exponential fog and a sky, all set per scene by the `environment` component (hour, day length, indoor, fog, tint); the same on Vulkan and WebGPU (`engine/core/include/Lighting.md`); the clock carries across areas and into saves; the editor has color pickers and light range rings; `Umbra --capture/--hour`. Example: the settlement at dusk with torches, the torch-lit dungeon with a glowing crystal. Verified headlessly (T84) and in software-rendered Vulkan and WebGPU pictures (`docs/testing/s6-lighting.md`); the GPU look and frame time on Windows are open. S5, the agent prompt (ADR 0008, `#UMBRA-80`; started by the user after P11), is implemented: one tool registry (`plugins/ai-authoring/modules/agent`: read, change and generate tools over the authoring documents and the ComfyUI generators) used by the editor's **Agent** window (a local Ollama model plans with the tools through a schema-forced JSON answer, read rounds and fixes; the plan is dry-run on working copies; after approval it runs and lands as one undo step) and by `UmbraTool mcp`, an MCP server on stdio for external coding agents (each change validated and saved at once; `--build` adds build and tests); `UmbraTool agent` plans without the editor. Verified headlessly (`docs/testing/s5-agent-prompt.md`); the real-model task set and the editor window on Windows are open. P11 Android (user decisions 2026-11-08: Android, the emulator first, touch + gamepad, landscape only; ADR 0009, `#UMBRA-79`) is implemented: the game builds as `libmain.so` inside a Gradle app (`plugins/android-export`, SDL's Java activity, `scripts\build-android.bat`), its assets and Vulkan 1.1 shaders travel in `game.pack` and are unpacked on first start, it saves and releases the Vulkan surface when sent to the background, and has touch controls (floating stick, camera drag, buttons, fight hotkeys) and gamepad sticks (`apps/umbra/Mobile.md`); verified headlessly (`docs/testing/p11-android.md`), the Gradle/NDK build and the emulator run on Windows open. S4 (user request 2026-11-08, ADR 0007, `#UMBRA-77`) is implemented: AI music (ACE-Step 1.5) and effects (Stable Audio 3) through the local ComfyUI (editor AI sound window, `UmbraTool sound`), Ogg music streamed per scene through the Music component with crossfades, skill/item sounds, a turn menu with target arrows and real-time hotkeys; verified headlessly (`docs/testing/s4-sound-and-menus.md`), real models and listening on Windows open. S3, the gameplay UI framework (user request 2026-11-08, ADR 0006, `#UMBRA-75`), is implemented: the game draws its own UI from `ui/*.ui.json` screen files with a skin and TTF fonts (`engine/ui`, the same on Vulkan and WebGPU); every game screen (HUD, start, pause, death, conversation, fight) plus inventory/equipment, skills and quest log come from the `rpg-ui` plugin and bind to `GameFlow::UiData`; keyboard, mouse and gamepad focus; the editor's Game UI window and `UmbraTool ui-preview`. Verified headlessly (47/47 GCC/Clang/ASan, 14/14 WebAssembly suites, check-web 13/13, screens checked as CPU-rendered pictures); the GPU, gamepad and editor checks on Windows are open (`docs/testing/s3-game-ui.md`). S2 (ADR 0005) moved game systems and tools into source plugins under `plugins/`, chosen by `project.json`. S1, project structure and components (user request 2026-11-08, ADR 0004), is implemented: base engine in `engine/`, base game in `game/` (rpg, turncombat, realtimecombat, encounter, story, game, policy, gameplay), one game per `projects/<name>/{assets,engine,game}` (`UMBRA_PROJECT`, default `reference`; content is now in `projects/reference/assets`); entities have `components` (built-in collider/interactable/portal/spawner plus project components: a schema in `assets/components` and C++ registered with `UMBRA_COMPONENT`); the editor has Add component and a Code window that opens the four layers in Visual Studio 2022; the web build loads `assets.bin` repacked by `serve-web.bat`, so editor changes show up without a rebuild. Verified headlessly on Linux (all suites, T76, T77); the Windows editor check is open. P10, the playable web export, is implemented (2026-11-08, user decisions O19: Chrome + Edge, single-threaded, Emscripten in the cloud): the game compiles to WebAssembly, draws with WebGPU through a JS library behind the same `Renderer`, HUD as HTML, IndexedDB saves with export/import, an offline service worker; 13 headless suites pass in WebAssembly and 9/9 browser checks in headless Chrome, but the picture is unverified until the user's check (`docs/testing/p10-web-export.md`). P9, the PC release gate, is in progress (2026-11-08): `Umbra --benchmark`, a long-run save/fight test, `UmbraTool package-check` with license notices, and run-time asset lookup are done and pass on Linux; the performance target is decided (ROADMAP O18, user 2026-11-08: the user's PC, RTX 3050 Laptop, 1920×1080, average ≤ 16.67 ms and p99 ≤ 33.3 ms per area; measured 3.47 ms / ≤ 6.2 ms); the remaining gate runs on Windows are open (`docs/testing/p9-release-gate.md`). P8B, P8C and P8D were verified on Windows with real models by the user on 2026-11-08 ("works, with limitations"). P8D, AI 3D props (user request, ADR 0003: TRELLIS.2 through the creator's exported ComfyUI workflow, props first, ~10 000 triangles, text or own picture), is implemented (2026-11-07; `docs/testing/p8d-ai-models.md`); `#UMBRA-62` fixed its reported limitations (any workflow name, the textured GLB from the save node, jobs survive closing the editor, no total time limit). P8C, AI texture generation (user request, ADR 0002: ComfyUI, tileable world surfaces, SDXL for an 8 GB GPU), is implemented (2026-11-07): materials with world-space texturing and mipmaps, a local-only ComfyUI bridge, a seamless step with a seam score, provenance and license warnings, the editor's AI texture window and `UmbraTool texture` (`docs/testing/p8c-ai-textures.md`); verified on Windows with SDXL by the user (2026-11-08). P8B, AI-assisted quest authoring, is implemented (2026-11-07): `Umbra::LocalAi` (a local-only Ollama bridge, default model `qwen2.5:7b`, user decision), `authoring::PlanDraft` (a draft only adds one quest and its conversation lines, is checked by the game's loader and played through with the real `GameSession` before approval), the editor's AI quest draft window and `UmbraTool draft` (`docs/testing/p8b-ai-authoring.md`). It is verified on Windows with a real Ollama model by the user (2026-11-08). Next: the P9 gate runs on Windows (including the P6/P7/P8A checklists). P8A, the learned turn-based enemy, is implemented (2026-11-07): `Umbra::Policy` (observation v1, action catalogue/masks, content fingerprint, built-in C++ network, async decider with conventional fallback), `tools/policy` (PPO on CPU through a C bridge to the native rules), and one accepted model for the bandit leader. It shows a learned low-player-HP attack preference in matched probes (+0.25 vs control −0.13), with competence equal to the conventional AI. The same recipe failed for a second seed (`docs/testing/p8a-learned-enemy.md`). Phase 7, the creator workflow, is implemented (2026-11-07): validated content documents with undo (`Umbra::Authoring`), `UmbraTool` (validate/inspect/schema/fight), the separate `UmbraEditor` (scenes with gizmo and picking, RPG content forms, Play), live RPG content reload in the game, a known-good build copy, `scripts/package.bat` and `docs/creator-guide.md` (`docs/testing/phase7-creator-workflow.md`). Its headless parts pass on Linux; the editor compiles but has not been run, and the Windows checklist is open. Phase 6, the complete offline reference RPG, is implemented (2026-11-07): conversations, a failable quest, XP and levels, save stones plus autosave with backups, roaming enemies with turn or real-time fights in a battle arena, and a death menu (`docs/testing/phase6-reference-game.md`). Its tests pass on Linux and on the user's Windows PC (27/27 MSVC Release, 2026-11-07); the first play test led to `#UMBRA-43` (save stone reach, a separate battle arena for every fight, hit flash); the full play-through is open. Phase 5, shared RPG rules and two combat modes, is verified headlessly on Linux. Windows gates are open for P3 (`docs/testing/phase3-assets-world.md`), P4 (`docs/testing/phase4-sandbox.md`, including GPU/audio/interactive) and P5 (`docs/testing/phase5-rules-combat.md`, build and CTest). On 2026-11-07 the MSVC Release build compiled with no warnings and 24/25 tests passed on the user's PC (one test exe was blocked from starting by Windows). W0 (2026-11-07): the rules build to WebAssembly unchanged with identical results, and WebGPU rendering plus localStorage worked in Microsoft Edge (`docs/adr/0001-web-backend.md`).

## 5. Superseded directions and explicit exclusions

| Earlier direction | Current treatment |
|---|---|
| Windows followed immediately by Android | PC → web → mobile. Original Android ARM64/API assumptions are historical, not a newly confirmed mobile specification. |
| Mandatory C# host and managed assembly reload | C++ gameplay first. C#, Lua, Python-as-gameplay, and other scripting runtimes are not initial dependencies. |
| End-user native plugins/mod ecosystem | Deferred. Source-level C++ extension does not require a public binary plugin ABI. |
| Learned aggression as a compulsory threshold rule | Rejected as the initial interpretation. Use learned preference plus behavior evaluation. |
| Combat modes as a compulsory unified scheduler | Independent controllers over shared rules; integration is optional. |
| Browser editor | Not requested. Export playable games only. |
| Multiplayer/cloud save/account infrastructure | Not initial scope. Optional future contributions are a separate feature. |
| Broad historical research's libraries and dated calendar charts | Not binding. This roadmap uses dependencies and evidence gates, not invented completion dates or effort estimates. |

Also outside initial scope: arbitrary mid-attack mode conversion; train-on-player-device/federated learning; training large foundation models from scratch; cloud-hosted runtime LLMs (a bundled local model is in scope since C20, ADR 0011); full visual scripting; seamless open-world streaming; networking replication; ray tracing; custom production physics; universal asset-format support; advanced ECS/jobs before measurements justify them.

These exclusions limit the initial product, not all future engine capabilities. New work requires a specific gameplay need and scope decision.

## 6. Proposed implementation defaults — not additional user approvals

The roadmap uses these engineering defaults to remain executable. They may be refined by small documented experiments without altering confirmed product requirements.

| ID | Default | Reason / revisit trigger |
|---|---|---|
| D01 | One authoritative simulation owner; immutable extraction/observation snapshots | Easier correctness and headless testing. Add parallel mutation only after profiling and a synchronization design. |
| D02 | Shared RPG definitions/state/rules; independently usable combat controllers | Satisfies C16 without duplicating rules. Test each mode without the other. |
| D03 | Integrate modes only at explicit safe encounter boundaries | Avoid converting unfinished attacks. Revisit only for a concrete game requirement. |
| D04 | Player saves and development checkpoints commit at safe simulation boundaries | Avoid serializing partially resolved effects/workers. Save UX is still a decision. |
| D05 | Native simulation + optional external Python training tools | Keep runtime C++ and reuse training ecosystems. Toolchain must be decided before P8A, not installed now. |
| D06 | ONNX Runtime as a candidate for a small feed-forward policy; CPU-first benchmark | A measured integration candidate, not a forced dependency or GPU commitment. |
| D07 | Gymnasium-style adapter; MaskablePPO as one candidate algorithm | Reset/step and discrete legality offer a tractable first experiment. A different algorithm may win the spike. |
| D08 | Local authoring provider/tool bridge; generated content becomes ordinary data | Keep authoring model size and service choices out of the game package. |
| D09 | Jolt behind the engine-owned `Umbra::Physics` adapter; do not implement production physics from scratch | Integrated in P4.1 (user-approved 2026-11-06); Jolt types never appear in engine headers. |
| D10 | Human-readable, versioned source content; small explicit descriptors; derived runtime assets where useful | Supports editing, validation, inspection, and AI changes without a universal reflection framework. |
| D11 | Future community contributions are consented experience records, centrally reviewed/retrained | Compatible with fixed shipped weights. The user has not approved this implementation or backend release. |
| D12 | Preserve Vulkan PC renderer; run an early small browser/backend feasibility experiment | Web export must be tested without prematurely replacing working graphics code. |

Provisional AI libraries are not dependencies of Phase 3. There is no instruction to add all candidates or pre-create every future module.

## 7. Architecture invariants

Runtime does not depend on editor. Authoritative simulation does not depend on rendering, an LLM, training tools, cloud services, or an account. Headless testing/training uses the same rules as gameplay, including the physics adapter when relevant.

Definitions are immutable authored data; instances hold changing state. Durable IDs identify saved objects/content; generational handles protect transient access. These are different concepts. Source data, runtime assets, saves, and model manifests have explicit versions and compatibility checks.

All actors request actions through the same validator. Learned AI may choose a legal but strategically poor action; the engine must not silently replace that choice just to manufacture the learned style. Fallback applies for failure, invalidity, or unavailability under a documented policy. Log fallback separately from policy behavior.

A graphical frame never waits on a model. Workers have bounded resources, immutable inputs, ownership tags, and controlled shutdown. The training runner may synchronously step the simulation without a renderer; that does not authorize blocking interactive rendering.

AI authoring proposes patches, not authoritative changes. Validation, preview, approval, reversible commit, and scenario tests remain distinct. Coding-agent code changes use normal source control, compilation, and tests. Both kinds of generation may fail.

Optional future services may consume exported records and supply approved model packages; the core must remain usable without them. Never add a cloud dependency under a generic “AI system” merely because optional online improvement is a future interest.

## 8. Definition of success

First achieve a complete offline reference RPG with conventional enemies. Then add the accepted learned turn-based policy and the quest/dialogue authoring workflow. The earlier conventional-AI game is a dependency-reduction milestone, not a replacement for the AI deliverables.

A proposed reference route is settlement → NPC conversation → quest acceptance → item/equipment and skill unlock → dungeon real-time encounter → separate turn-based encounter → quest completion/reward → save/exit/reload. It demonstrates the required content, not a locked story or camera choice.

The final PC gate includes a clean install, both independently testable modes, conventional fallback, local packaged policy, authoring preview/undo, useful error reporting, local save recovery, and measured performance on a declared reference machine. Web and mobile are later export milestones; neither changes the desktop-editor requirement.

## 9. Remaining choices and decision timing

None of these is a reason to halt basic Assets and World work.

| Choice still open | Decide before |
|---|---|
| Art style (camera = third-person orbit, collision = Jolt: decided 2026-11-06; skeletal animation with Meshy/CC0 rigs and graphs: S8, 2026-11-10) | When reference presentation needs it |
| One enemy instance vs reusable archetype; encounter arrangement (decided 2026-11-07: area pools, roaming enemies, 1–3 per fight in a separate battle arena) | P5/P6 reference content |
| ~~Turn order/AP, real-time cost timing, effect clocks, mode transitions~~ decided 2026-11-07: speed rounds with one action; cost paid at commit; statuses/cooldowns end with the encounter, modes change only between encounters | done |
| Skill-tree respec (the in-game skill-tree screen exists since S3; quest failure, save stones/autosave and overflow decided 2026-11-07, see ROADMAP O07) | P9 reference polish |
| Exact source schema/descriptor implementation, serialization library | Relevant P3/P5 data slice |
| ~~Training stack, algorithm, inference runtime~~ decided 2026-11-07: PyTorch on CPU, PPO with masks, built-in C++ MLP; reward weight 1.0 and thresholds in `tools/policy/acceptance.json`; difficulty profile still open | P8A follow-up |
| ~~Local authoring model, bridge, download size/license~~ decided 2026-11-07: Ollama on the creator's PC, `qwen2.5:7b` (4.7 GB, Apache-2.0, installed by the creator), editor + tool; real-model quality still unmeasured. Optional cloud credential/cost UX still open | P8B follow-up |
| ~~Reference PC, resolution, workload, frame budget~~ decided 2026-11-08 (ROADMAP O18): user's PC, 1920×1080, average ≤ 16.67 ms, p99 ≤ 33.3 ms | done |
| ~~Browser support set~~ decided 2026-11-08 (ROADMAP O19): Chrome + Edge, single-threaded; HLSL → WGSL automation still open | done |
| ~~First mobile OS/device tier~~ decided 2026-11-08 (ADR 0009): Android, the emulator first, then arm64 phones; landscape; touch + gamepad. Phone model list still open | P11.5 |
| Community collection/retraining/update design and service operations | Separately approved post-release track |

The roadmap contains an open decision register with reversible defaults and revisit triggers. The implementing agent should resolve only the choices required for its current slice and document the result.

## 10. Maintaining this context

Change confirmed requirements only after a user decision; record which prior decision it supersedes. Update documentary status only with dated evidence, preserving the distinction between inherited reports and new verification. Put detailed command logs and per-task progress in the roadmap/evidence system, not this stable context brief.

Keep these three files in sync. Do not repeatedly append session transcripts or duplicate the entire roadmap here. A small corrected context is more useful than an unbounded memory dump.
