# Umbra — implementation roadmap and verification register

**Version:** 1.2 · **Requirements snapshot:** 2026-11-01, user answers 1–23  
**Last implementation status record:** [`current-engine-status.md`](current-engine-status.md), 2026-09-02; Phases 1 and 2 documented complete  
**Repository verification:** partial B0 on 2026-11-01 — source inspected; Core/Diagnostics/Platform tests run on Linux/GCC only; Windows presets not run. See [`testing/b0-baseline-reconciliation.md`](testing/b0-baseline-reconciliation.md)  
**Companions:** [Project context](CONTEXT.md) · [Claude working instructions](../CLAUDE.md)

This is the updated working plan, not a completion certificate. It replaces the future sequencing in [`custom-3d-game-engine-roadmap.md`](custom-3d-game-engine-roadmap.md) (kept as history) and the earlier `umbra-rpg-roadmap-baseline-2026-11-01.md` (not in the repository) while preserving its core structure and T01–T38 test identifiers. Technical defaults and new acceptance policies below are proposed engineering design, not additional user approvals. Confirmed product scope is in CONTEXT, C01–C18.

The three formerly open decisions are resolved: the combat modes are independently usable and optionally integrated; the reference game uses both; the first learned policy is turn-based; low-player-HP aggression is a learned preference. No new product question is required before auditing the existing code and continuing Phase 3.

## Navigation

- [0. Execution dashboard](#0-execution-dashboard)
- [1. Product charter and scope](#1-product-charter-and-scope)
- [2. Observable definition of done](#2-observable-definition-of-done)
- [3. Architecture and ownership](#3-architecture-and-ownership)
- [4. Shared RPG simulation](#4-shared-rpg-simulation)
- [5. Learned enemy policies](#5-learned-enemy-policies)
- [6. AI-assisted authoring](#6-ai-assisted-authoring)
- [7. Optional community improvement](#7-optional-community-improvement)
- [8. Milestones and work packages](#8-milestones-and-work-packages)
- [9. Fast iteration and agent tooling](#9-fast-iteration-and-agent-tooling)
- [10. Acceptance and recovery matrix](#10-acceptance-and-recovery-matrix)
- [11. Performance and release metrics](#11-performance-and-release-metrics)
- [12. Immediate implementation sequence](#12-immediate-implementation-sequence)
- [13. Decision and risk registers](#13-decision-and-risk-registers)
- [14. Evidence and handoff templates](#14-evidence-and-handoff-templates)
- [15. Sources and change history](#15-sources-and-change-history)

## 0. Execution dashboard

**First action for a new agent:** finish B0 — the Windows-side checks are still open (see the B0 record). Re-inspect the checkout; if it has advanced past this dashboard, preserve completed work and select the earliest genuinely unmet task. Do not blindly implement P3.1 again.

**Current next task:** Plan 4a, the painted table (`design/plans/2026-11-15-painted-table-plan-4a.md`). Plan 3, the model's lines, is **implemented** (`#UMBRA-162`-`#UMBRA-169`, `testing/s29-model-lines.md`; the real-model check on Windows open, §4); guest colours and tinted animated pawn figures are **implemented** (`#UMBRA-158`-`#UMBRA-160`, `testing/s28b-pawn-figures.md`). Also open: the user's Windows play check of plans 2, 2b and 2c (`testing/s26-board-on-screen.md` §4, `testing/s27-goals-and-board.md` §4, `testing/s28-the-screen.md` §4), then plan 3, the model's lines. Plan 2c (the screen, `#UMBRA-146`-`#UMBRA-156`, 2026-11-15) is **implemented**: no ally goals (user), walls wherever a guest cannot pass, a turn menu, walking clicks with sliding steps and Stay/Back at doors, input by state, inspect, a save menu, bots that think and slide, a following camera with drag and Focus, labels and numbered group rings; Linux GCC/Clang/ASan 55/55. Plan 2b (goals and a bigger board, `#UMBRA-137`-`#UMBRA-145`, 2026-11-15) is **implemented**: omens removed, personal goals decide who turns at midnight, a 9×9 board of 70 rooms with seven start rooms, chats and groups between any guests, bots with a purpose, every band of the soak holds, save version 2; Linux GCC/Clang/ASan 55/55. Plan 2 (the board on screen, `#UMBRA-121`-`#UMBRA-131`, 2026-11-14) is **implemented**: the conversation rule and push back in the rules, the `board` module (what the player sees, narration, picking), `LastCurtain.exe` (a table view, the player's turn, paced bot turns with captions, chat, event, dice, omen, battle, Turn and end screens, start screen and saves), all on Linux GCC/Clang/ASan 55/55 with SwiftShader pictures. Plan 1 (the night as a simulation, `#UMBRA-104`-`#UMBRA-116`) is accepted (MSVC matched Linux). Design: `design/game-design.md`, `design/cast.md`, `design/2026-11-14-board-on-screen.md`; state in `design/HANDOFF.md`. S21-S24 stay as the `living-town` plugin (open checks: `testing/s24-storyteller.md` §4, `testing/s23-built-in-model.md` §2f step 4, `testing/s22-memory-chronicle-rumours.md` §3)

| Gate | Dependency | Status (updated 2026-11-07) | Evidence / exit summary |
|---|---|---|---|
| P1 Foundation | — | Documented complete, 2026-09-02; interactive checklist unsigned | Status doc reports tests, lifecycle, diagnostics, CI and ownership checks. But `testing/phase1-smoke-test.md` is still "Partial" with every checklist item unticked. Core/Diagnostics/Platform tests re-run on Linux/GCC 2026-11-01 (pass). |
| P2 Rendering | P1 | Documented complete, 2026-09-02 | `testing/phase2-rendering-smoke-test.md` records user-confirmed pass of the textured glTF primitive, validation, resize recovery and RenderDoc checks. Not re-run since. |
| B0 Baseline reconciliation | Existing checkout | In progress | Source inspection and Linux test run done 2026-11-01. Open: Windows MSVC/clang-cl/ASan build + CTest, CI status, Phase 1 checklist sign-off. |
| P3 Assets and World | B0; existing P1/P2 | Implemented 2026-11-01; headless verified on Linux; **Windows/GPU gate open** | All tasks P3.1–P3.8 implemented. Evidence and the remaining checklist: `testing/phase3-assets-world.md`. |
| W0 Early web feasibility | P3 core contracts; minimal rendering sample | Done 2026-11-07: rules → WebAssembly with exact native parity; WebGPU rendering of the settlement and localStorage persistence verified in Microsoft Edge (Windows, Intel GPU) | `samples/web`, ADR `docs/adr/0001-web-backend.md`. Still untested: other browsers, Emscripten, automatic HLSL→WGSL (toolchain downloads blocked in the agent environment). |
| P4 Playable 3D sandbox | P3 | Implemented 2026-11-06; headless verified on Linux; **Windows/GPU/interactive gate open** | All tasks P4.1–P4.5 implemented. Evidence and checklists: `testing/phase4-sandbox.md`. |
| P5 Shared rules and combat modes | P3; P4 for graphical integration | Implemented 2026-11-07; headless verified on Linux; Windows gate open | P5.1–P5.8 implemented; turn-only, real-time-only and combined fixtures pass. Not yet wired into the 3D game (P6). Evidence: `testing/phase5-rules-combat.md`. |
| P6 Complete offline reference game | P4/P5 | Implemented 2026-11-07; Linux and Windows CTest pass; first play test fixed (`#UMBRA-43`); full play-through open | P6.1–P6.5: conversation, quest with failure, XP/levels, save/autosave with backup, roaming enemies, in-place turn and real-time fights, death menu. Packaging from a clean install (T43/T45) remains for P9. Evidence: `testing/phase6-reference-game.md`. |
| P7 Creator workflow | P3/P5/P6 contracts | Implemented 2026-11-07; headless parts verified on Linux; editor compiled, not yet run; Windows checklist open | P7.1–P7.6: shared validated documents with undo (`Umbra::Authoring`), `UmbraTool`, `UmbraEditor` (scenes + RPG forms, gizmo, Play), live RPG content reload, known-good build, package script, creator guide. Evidence: `testing/phase7-creator-workflow.md`. |
| P8A Learned turn-based enemy | P5 rules, P7 tools | Implemented 2026-11-07; one model accepted under declared thresholds; recipe 1 of 2 seeds; Windows run open | Built-in C++ network (user decision), PPO trained on CPU through the native rules, matched-probe preference gap +0.25 vs control −0.13, competence equal to the conventional AI (content-limited). Evidence: `testing/p8a-learned-enemy.md`. |
| P8B AI authoring | P7 schema/patch/validation capability | Implemented 2026-11-07; verified on Linux, and **on Windows with a real Ollama model by the user (2026-11-08: works, with limitations)** | `Umbra::LocalAi` (local-only Ollama bridge, streamed, schema-constrained, cancel/limits), `authoring::PlanDraft` (additive scope, game loader, play-through with the real `GameSession`), editor "AI quest draft" window and `UmbraTool draft`. Evidence: `testing/p8b-ai-authoring.md`. |
| P8C AI texture generation | P7 authoring, P8B local-AI bridge | Implemented 2026-11-07; verified on Linux, and **on Windows with ComfyUI/SDXL by the user (2026-11-08: works, with limitations)** | Materials with world-scale texturing and mipmaps; ComfyUI bridge; seamless step and seam score; provenance and license warnings; editor AI texture window with in-scene preview; `UmbraTool texture`. Plan `adr/0002-ai-texture-generation.md`, evidence `testing/p8c-ai-textures.md`. |
| P8D AI 3D models | P8C ComfyUI bridge | Implemented 2026-11-07; **verified on Windows with TRELLIS.2 by the user (2026-11-08)**; the reported limitations fixed in `#UMBRA-62` (any workflow name, the textured GLB, survives closing the editor, no time limit) | Text → SDXL concept or own picture → the creator's exported TRELLIS.2 workflow (ComfyUI) → GLB with provenance → placed as an undoable scene edit; `UmbraTool model`. ADR `adr/0003-ai-model-generation.md`, evidence `testing/p8d-ai-models.md`. |
| P9 PC release | P6/P7/P8A/P8B | **In progress** (2026-11-08): release tools built and passing on Linux (benchmark, long-run test, package check, license notices); the Windows gate runs are open | Clean offline install, complete game, authoring workflow, measured reference-PC gate. Evidence `testing/p9-release-gate.md`. |
| P10 Playable web export | P9; W0 decisions | **Implemented 2026-11-08** (started before the P9 Windows runs finished, at the user's request); verified headlessly (13/13 suites in WebAssembly, 11/11 browser checks) and **renders on the user's GPU**; Chrome/Edge play-through and offline install to confirm | Browser game and local policy, durable-save UX, offline installation readiness. Evidence `testing/p10-web-export.md`. |
| S1 Project structure and components | P10 | **Implemented 2026-11-08** (user request after P10): `engine/` / `game/` / `projects/<name>/{assets,engine,game}`, entity components with project C++ components, editor Code window (VS 2022) and Add component; verified headlessly (T77, all suites); editor use on Windows to confirm | ADR 0004; `projects/README.md`; `engine/scene/include/Components.md`. |
| S2 Source plugins | S1 | **Implemented 2026-11-08** (user request): game systems and tools in `plugins/<name>/` chosen by `project.json`; editor windows and tool commands register themselves; layered assets; three plugin sets verified (T78); Windows check open | ADR 0005; `plugins/README.md`. |
| S3 Gameplay UI framework | S2 | **Implemented 2026-11-08** (user request, `#UMBRA-75`): screens as data files drawn by the engine on PC and web, all game UI replaced, inventory/skills/quest log, editor Game UI window, `UmbraTool ui-preview`; verified headlessly (T79, check-web 13/13); GPU, gamepad and editor check on Windows open | ADR 0006; `engine/ui/include/UI.md`; evidence `testing/s3-game-ui.md`. |
| S4 AI sound, music, turn menu, hotkeys | S3 | **Implemented 2026-11-08** (user request, `#UMBRA-77`): AI music (ACE-Step 1.5) and effects (Stable Audio 3) through ComfyUI, Ogg music with crossfades per scene, skill/item sounds, turn menu with target arrows, real-time hotkeys and their screen; verified headlessly (T80, T81, check-web 13/13); real models and listening on Windows open | ADR 0007; evidence `testing/s4-sound-and-menus.md`. |
| S5 Agent prompt (editor Agent window, MCP server) | S4 | **Implemented 2026-11-08** (started by the user after P11, `#UMBRA-80`): one tool registry over the authoring documents and the ComfyUI generators, a local Ollama planner (schema-forced plan, read rounds, dry run, fixes), runner with one undo step, editor Agent window, `UmbraTool agent`, `UmbraTool mcp` (stdio, + build/tests); verified headlessly (T83); real-model task set and editor check on Windows open | ADR 0008; `Agent.md`; evidence `testing/s5-agent-prompt.md`. |
| S6 Lighting & atmosphere | S5 | **Verified by the user 2026-11-09** (review on Windows: "everything works fine"); implemented 2026-11-09 (`#UMBRA-82`; user decisions: time saved, 12-minute day starting at dusk, moonlit readable nights): linear light, hemisphere sky light, sun/moon with a 2048² shadow map, up to 16 flickering point lights, emissive surfaces, exponential fog, sky, `light`/`environment` components, editor color picker and light rings, time kept across areas and in saves; Vulkan and WebGPU pictures checked in the cloud (SwiftShader); verified headlessly (T84); frame time on the user's PC open | `engine/core/include/Lighting.md`; evidence `testing/s6-lighting.md`. |
| S7 Post-processing & custom shaders | S6 | **Verified by the user 2026-11-09** (same review); implemented 2026-11-09 (`#UMBRA-83`; user decisions: SPIR-V converted to WGSL at build time with naga, reduced post on phones): HDR scene picture, 6-level bloom (4 from quarter size on Android), ACES tonemap, a baked 16³ color table (LUT image + temperature, saturation, contrast), vignette, `post` component; custom material surface shaders (`*.surface.hlsl`, Vulkan pipeline per shader, WGSL by DXC + naga for the browser, errors at the shader's own line); the same on Vulkan and WebGPU; verified headlessly (T85, T86) and in software-rendered pictures; the user's GPU check (with S6) open | `engine/core/include/Post.md`, `engine/render/shaders/CustomShaders.md`; evidence `testing/s7-post-and-shaders.md`. |
| S8 Skeletal animation & rigging | S5 | **Implemented 2026-11-10** (`#UMBRA-84`; user decisions: Meshy rig + clips for the hero and bandit, a CC0 animated wolf, AI rigging and text-to-motion through a local ComfyUI): glTF skins and clips imported and cached, GPU skinning on Vulkan and WebGPU (joint storage buffer, skinned scene/shadow/surface pipelines), animation graphs (`*.anim.json`: first matching state, crossfade, triggers, speed-driven rate), `animation` component and RPG actor field; the hero, bandits and wolves walk, run, attack, flinch and fall in the game; the elder stays unrigged (Meshy pose estimation failed on her robe); AI rig (UniRig) and motion (HY-Motion) from the creator's exported ComfyUI workflows (editor AI animation window, `UmbraTool rig`/`motion`, FBX converted by Blender), tested with a stand-in ComfyUI; verified headlessly (T87, T88) and in software-rendered pictures; the user's GPU check and a real UniRig/HY-Motion run open | `engine/core/include/Skeleton.md`, `engine/scene/include/AnimGraph.md`, `plugins/ai-authoring/modules/localai/include/AnimationGen.md`; evidence `testing/s8-skeletal-animation.md`. |
| S9 Animation events, SFX & VFX | S8 | **Implemented 2026-11-10** (`#UMBRA-86`): clip events with a sound and an effect, fired once per crossing; a CPU particle system (`vfx/*.vfx.json`, repeatable per seed) drawn as instanced camera-facing squares on Vulkan and WebGPU; the `vfx` component; blows land on the attacker's `hit` event; damage numbers, hit-stop, camera shake, red flash (`GameFeel`); skills, items and statuses name an `effect`; example: fire on the torches, step dust and timed footstep, swing and hit sounds, a fireball skill, poison bubbles, a potion glow; verified headlessly (T89) and in software-rendered pictures; the user's check with a GPU and sound is open | `engine/core/include/Particles.md`, `engine/scene/include/AnimGraph.md` "Events", `apps/umbra/GameFeel.md`, `apps/umbra/GameFlow.md` "Blows"; evidence `testing/s9-events-sfx-vfx.md`. |
| S10 RPG systems | S5 | **Re-scoped 2026-11-10** (ADR 0011): money and trade, dialogue choices with consequences are built inside S21-S24; the rest waits for a game that needs it | Gold and shops, loot, chests/doors/levers, dialogue choices with consequences; example: merchant Tobin, locked chest and key, the elder's choice. |
| S18 Party & characters | S10 | **Re-scoped 2026-11-10** (ADR 0011): a resident accompanying the player comes with S21-S24; the full party waits | Up to 4 members, companions recruited by dialogue, per-member stats/equipment/skills, turn and real-time party fights, character screen, party in saves; example: recruit Mara the hunter. |
| S11 Cutscenes | S9, S10 | Planned | `*.cutscene.json` timeline (camera, actors, lines, sound, fades); example: intro fly-over to the elder, bandit leader reveal. |
| S19 Resource previewers | S11 | Planned (user request 2026-11-10) | Preview window for every clickable asset except code: sound/music player, image, model turntable, material sphere, UI screen, RPG content cards; also the S12 thumbnail renderer. |
| S20 Editor settings | S19 | Planned (user request 2026-11-10) | Settings window per user: colour scheme, UI scale, hotkey rebinding; diagnostics overlay on/off, frame graph, validation, log level. |
| S21 World tick & residents | S9 | **Implemented 2026-11-10** (`#UMBRA-91`): the `living-town` plugin (`Umbra::Town`, headless, integers only): town files (`town/*.town.json`: needs, places, activities, residents with schedules, money and bonds), a 10-minute tick, validated intents with a rule-based chooser, experience and levels, events, the whole state one copyable value (forks), saved in the player's save (`extras.town`); `UmbraTool town-run`; in the game the town follows the world's clock and residents stand at their places' markers; example: seven residents in the settlement; verified headlessly (T90, golden fingerprint on GCC/Clang/ASan; about 9,000 days per second); MSVC and the user's check open | `plugins/living-town/modules/town/include/Town.md`; evidence `testing/s21-world-tick-residents.md`. |
| S22 Memory, chronicle & rumours | S21 | **Implemented 2026-11-11** (`#UMBRA-92`): facts with who saw, who knows and who told the player; rumours pass along residents' talks; each resident's regard for the player changes when they learn of a kept or skipped favour; favours in the town file; template lines for every fact; talk with **E**, chronicle screen on **L** (what the player was told); saved with the town; verified headlessly (T91); MSVC and the user's check open (`plugins/living-town/modules/town/include/Town.md`, `testing/s22-memory-chronicle-rumours.md`). Plan was: | Event log, per-resident memory, rumours, chronicle screen, template wording; example: a skipped favour is mentioned by another resident two days later. |
| S23 Built-in model | S22 | **Implemented; measured 2026-11-12** (Qwen3 4B on the graphics card, 0.8 s a line, `testing/s23-built-in-model.md` §2g; in-game check and packaging open) (`#UMBRA-93`-`#UMBRA-100`): `town::Mind` with llama.cpp behind it (vcpkg `llama-cpp`, feature `town-model`), grammar-constrained requests, a line guard of extendable checks, a worker thread with late and stale answers dropped, accepted words kept in the save, `UmbraTool town-mind` to measure a model file; verified with a scripted stand-in (T92) and a random-weights model through llama.cpp; **no real model has run yet**: the model choice and numbers are the user's run (`plugins/living-town/modules/mind/include/Mind.md`, `testing/s23-built-in-model.md` §3). Plan was (ADR 0011; the model is required in a living-town game, PC only for now) | llama.cpp plugin on a worker thread; briefs and schema-forced answers; the model chooses among legal intents, proposes authored event kinds and words lines; everything through the validator; output stored in saves; stand-in model for tests; model pack in packaging. |
| S24 Storyteller & outcome plan | S23 | **Implemented 2026-11-12** (`#UMBRA-101`): trouble (ill, theft, feud, leave, death) proposed by the town's rules; the plan file `town/*.plan.json` (beats with windows and outcomes, never, allow, pacing) checked on load; the storyteller (filter, the beat first, force on the last day, nudge by rehearsing forked towns); `UmbraTool town-soak` and `town-run --plan`; the game keeps the story in the save; editor Town plot window; `pronoun` per resident. 1,000 seeds of the placeholder plot: always finished, nothing against the plan, 69 roads, 4-10% of beats forced (19-29% without rehearsal). T93; MSVC and the user's check open (`testing/s24-storyteller.md` §4). Limits: beats are town trouble only, not yet tied to quests or the player. Plan was (ADR 0011) | Developer-authored outcome plan (beats, windows, allowed/required/forbidden outcomes, pacing); storyteller filters proposals, looks ahead over forked worlds and forces beats; warnings; editor view; soak test over many seeds with an outcome report. |
| S25 The Last Curtain, plan 1: the night as a simulation (replaces "the town", ADR 0012) | S24 | Implemented 2026-11-14 (`#UMBRA-104`-`#UMBRA-116`; Linux GCC 55/55; MSVC tests and soak matched Linux; `testing/s25-night-simulation.md`) | One town, seven residents, a short main plot with managed variation; two playthroughs differ and both finish the plot. |
| S28 The Last Curtain, plan 2c: the screen | S27 | Implemented 2026-11-15 (`#UMBRA-146`-`#UMBRA-156`; Linux GCC/Clang/ASan 55/55; SwiftShader pictures; Windows play check open, `testing/s28-the-screen.md` §4) | The turn plays by menu and walking clicks; input by state; inspect; save menu; the camera follows; bots think and slide. |
| S28b The Last Curtain: guest colours and pawn figures | S28 | Implemented 2026-11-15 (`#UMBRA-158`-`#UMBRA-160`; T130, T131; Linux GCC/Clang/ASan 55/55; Windows look check open, `testing/s28b-pawn-figures.md` §4) | Each guest has a colour; pawns are tinted animated figures (one guest model, one Taken model). |
| S29 The Last Curtain, plan 3: the model's lines | S28b | Implemented 2026-11-15 (`#UMBRA-162`-`#UMBRA-169`; T132-T136; Linux GCC/Clang/ASan 55/55; stand-in model only; real-model check on Windows open, `testing/s29-model-lines.md` §4) | The built-in model rewords captions, chat questions, goal lines, the Turn and the Taken's lines in each guest's voice; the game waits up to 3 s, else the written line. |
| S30 The Last Curtain, plan 4a: the painted table and the night's mood | S29 | Implemented 2026-11-15 (`#UMBRA-170`-`#UMBRA-173`, `#UMBRA-176`, pictures `#UMBRA-182`, `#UMBRA-188`; T140-T145; SwiftShader pictures; Windows look check open, `testing/s30-painted-table.md` §4) | A painted tile per room and a picture per event, cards, items and portraits (SDXL on the user's PC); feature rooms framed gold; the mood creeps from 19:00 and turns to blood at the Turn, in shaders. |
| S31 The Last Curtain, plans 4b and 4c: a room at full size and battles in it | S30 | Implemented 2026-11-15 (`#UMBRA-174`, `#UMBRA-175`; T144; Windows play check open, `testing/s31-rooms-and-battles.md` §4) | Look shows the player's room at full size from a fixed corner; battles in sight play there. |
| S32 The Last Curtain: persistent rooms and the pace (the user's feedback) | S31 | Implemented 2026-11-15 (`#UMBRA-178`-`#UMBRA-180`; T146-T149; soak holds; Windows play check open, `testing/s32-persistent-rooms-and-pace.md` §4) | Persistent events and traps with clear conditions; a turn arrow; a room panel; tap to continue; Skip; moments at full size; `options.csv`. |
| S33 The Last Curtain: the room's flow (the user's second feedback) | S32 | Implemented 2026-11-15 (`#UMBRA-183`-`#UMBRA-187`; T150-T152 and a UI click test; Windows play check open, `testing/s33-room-flow.md` §4) | Clicks reach HUD and caption buttons; bots' walks stay on the table; rest after a room; the full view shows the 3×3; a room plays arrival, card, tumbling die, plain result. |
| S34 The Last Curtain: the room view as a play mode, a new move rule (the user's third feedback) | S33 | Implemented 2026-11-15 (`#UMBRA-192`-`#UMBRA-199`; T153-T162; soak holds after rebalancing three Taken forms; save version 4; Windows play check open, `testing/s34-third-feedback.md` §4) | Room view with clicks, zoom, drag; moves as the shortest way back; previews of bots' deeds; visible fights; chat replies; markers; readable walls; the acting guest in the top bar. |
| S35 The Last Curtain: the fourth feedback (bots walk in before telling, dice, labels, midnight sight) | S34 | Implemented 2026-11-15 (`#UMBRA-203`-`#UMBRA-209`; T164-T171; soak holds; save version 5; Windows play check open, `testing/s35-fourth-feedback.md` §4) | Bots walk in then tell, with their rolls; dice stop and hold; names, portraits, room names, goal glow; the Taken and company know each other's places after midnight; no pacing bots; the Taken's story. |
| S36 The Last Curtain: floor plaques, the Saigon sky, guest pages | S35 | Implemented 2026-11-15 (`#UMBRA-211`-`#UMBRA-213`; T212-T214; Windows play check open, `testing/s36-plaques-sky-guest-pages.md` §4) | Room names as brass plaques on the floors; a painted Saigon sky behind the board, dusk before midnight and a red moon after; the start screen shows portraits and each guest's page before the night begins. |
| S37 The Last Curtain: the fifth feedback (cutscenes, turns, the hunt, sounds) | S36 | Implemented 2026-11-16 (`#UMBRA-214`-`#UMBRA-216`; T215-T227; Windows play check open, `testing/s37-fifth-feedback.md` §4) | Scenes on the board at the opening, midnight and the end; turn banners and order; the Taken's HP and a staged fall; clickable dialog boxes; a light switch; nine AI sounds. |
| S38 The Last Curtain: the battle royale (branch `battle-royale`) | S37 | Implemented 2026-11-16 (`#UMBRA-217`-`#UMBRA-223`; T228-T256; soak holds; save version 6; Windows play check open, `testing/s38-battle-royale.md` §4) | One map, guests spawned apart and fighting at any hour; the dead play on as ghosts of the house; an unkillable killer from midnight; quests raced for powers; persuading with requests; natures; nights until one is left. |
| S39 The Last Curtain: presentation and UX round (branch `battle-royale`) | S38 | Implemented 2026-11-17 (`#UMBRA-226`-`#UMBRA-231`; T259-T266; Windows play check open, `testing/s39-presentation-round.md` §4) | One spotlight for camera and top bar; bots' event cards and rolls; deaths announced; bots' chats shown; blows told; grouped menu; quest book. Next: hook research, then the fight view, rerolls and the stage API (user's order). |
| S40 The Last Curtain: faction matches, plan 1, the 3v3 core (branch `factions`) | S39 | Implemented/unverified 2026-11-19 (`#UMBRA-251`-`#UMBRA-260`; T274-T307; headless only; balance holds since `#UMBRA-262`, guardians beatable in one battle, `testing/s40-faction-match-core.md` §2) | Two factions, essence carried to a moving stage and Performed, three-round battles, board abilities, mirrored lane board, monsters and guardians, bots at three levels, the soak. Next: Plan 2, the screens. |
| S42 The Last Curtain: plan 4, tiles, group battles, the told match, the café hub (branch `factions`) | S41 | Implemented/unverified 2026-11-21 (`#UMBRA-291`-`#UMBRA-297`; T353-T367; Linux GCC pass; soak holds; Windows checks open, `testing/s42-tiles-groups-cafe.md` §4) | Walls as tiles, shared tiles, group battles, a match told blow by blow, button states, the café hub with a timed serving shift and a saved career. Next: the ghosts' art, the user's checks. |
| S44 The Last Curtain, campaign plan 1: the Vietnamese model test, the voice test, the fonts | S42; campaign slice 1 design (approved 2026-11-23) | Implemented/unverified 2026-11-23 (`#UMBRA-324`-`#UMBRA-330`; Linux GCC; tiny random model through real llama.cpp; the user's model runs, Vietnamese review and listening open, `testing/s44-vietnamese-model.md` §4) | Be Vietnam Pro and Playfair Display carry every Vietnamese letter; voice sheets and a 20-line café test set; ten Vietnamese line checks; `UmbraTool talk-mind` (report per model, comparison); `tools/voice/vieneu_lines.py` with Tí's pitch shift. Next: the user's Arcee-VyLinh run and pick; campaign plan 2 (rules) can start meanwhile. |
| S45 The analog screen (CRT and VHS) | S7 post-processing; S44 | Implemented/unverified 2026-11-23 (`#UMBRA-333`; user request; Linux GCC; SwiftShader pictures; Windows look, resize and frame-rate check open, `testing/s45-analog-screen.md` §4) | A last pass over the whole frame, menus and text included: curved glass, scanlines, colour fringe (CRT); shaking lines, a rolling tracking band, colour bleed, grain (VHS). Strengths in the post settings; The Last Curtain turns it on by default with two pause-menu switches, saved. |
| S46 The Last Curtain, campaign plan 2: the fight rules | S44; campaign slice 1 design | Implemented/unverified 2026-11-23 (`#UMBRA-334`-`#UMBRA-340`; Linux GCC 59/59, faction soak 49.1% / comebacks 26.5%; MSVC and the user's faction-match check open, `testing/s46-campaign-rules.md` §4) | Modes (faction, campaign); one Speed order with monsters in it; falls and revive; biding and AP; the gold knot and freeing; the bosses' win; the rite; save version 4. Plans 3–7 written (`design/plans/2026-11-23-campaign-plan-{3..7}-*.md`). |
| S47 The Last Curtain, campaign plan 3: dialog scenes | S46; S7 UI | Implemented/unverified 2026-11-23 (`#UMBRA-341`-`#UMBRA-346`; Linux GCC; SwiftShader picture; the user's reading of the prologue and the Windows click-through open, `testing/s47-dialog-scenes.md` §4) | Engine images flip and tint; `modules/campaign` (cast, validated scenes, `ScenePlayer`); the dialog screen and log; `LastCurtain --scene`; the prologue in Vietnamese (draft). Next: campaign plan 4, stages. |
| S48 The Last Curtain, campaign plan 4: fights you can play | S46, S47 | Implemented/unverified 2026-11-24 (`#UMBRA-352`-`#UMBRA-364`; Linux GCC 59/59 plus the soaks; the user's play check and the drafts' reading open, `testing/s48-campaign-stages.md` §4) | Rule additions (hasten, effects, drops, use flags, gimmicks marked a round ahead), stage files and `BuildStage`, behaviours, scenes inside a fight, the slice's six fights with three skirmish twists, `LastCurtain --stage`, `build stage`, `UmbraTool campaign-soak` with bands. Next: campaign plan 5, the battle HUD. |
| S49 The Last Curtain, campaign plan 5: the battle HUD | S48; S3 UI | Implemented/unverified 2026-11-24 (`#UMBRA-365`-`#UMBRA-373`; Linux GCC 59/59; the user's mouse-only and keyboard-only play open, `testing/s49-battle-hud.md` §4) | Words out of the screens (Vietnamese fight words, draft), large text, a key file, the campaign HUD (goals, strip, hint, toasts, card), the command menu with sub-menus, targets and the back stack, previews, cast names, strings and telegraphs on the board. Next: campaign plan 6, the café. |
| S41 The Last Curtain: faction matches, plans 2 and 3, the screens and polish (branch `factions`) | S40 | Implemented/unverified 2026-11-20 (`#UMBRA-264`-`#UMBRA-289`; T310-T349; Linux GCC/Clang/ASan pass; soak holds; Windows play check open, `testing/s41-faction-match-screens.md` §4) | Spawned monsters and hunters, treasure cards, returning rooms, level cards, event dice; the match playable in `LastCurtain` with the prototype layout, ComfyUI art, motions, sounds, camera, bubbles and room zoom. Next: the user's Windows check. |
| S27 The Last Curtain, plan 2b: goals and a bigger board | S26 | Implemented 2026-11-15 (`#UMBRA-137`-`#UMBRA-145`; Linux GCC/Clang/ASan 55/55; soak holds on three seed ranges; Windows check open, `testing/s27-goals-and-board.md` §4) | Personal goals decide who turns; 70 rooms on a 9×9 board; bots work toward goals and recruit each other; every goal 45-65%. |
| S26 The Last Curtain, plan 2: the board on screen | S25 | Implemented 2026-11-14 (`#UMBRA-121`-`#UMBRA-131`; Linux GCC/Clang/ASan 55/55; SwiftShader pictures; Windows and play check open, `testing/s26-board-on-screen.md` §4) | `LastCurtain.exe`: a table view of the night, the player's turn, bot turns with captions, chat, event, dice, omen, battle, Turn and end screens, saves and Continue. |
| S12 Prefabs & drag-and-drop | S20 | Planned | Prefabs with overrides, thumbnails, drag into the view, multi-select, duplicate, snapping; example: ten torches from one prefab. |
| S13 Play in editor | S12 | Planned | Game inside the editor view, Stop restores the scene; example: walk and fight in the editor. |
| S14 Graph editors | S13, S10 | Planned | Conversation, quest, cutscene and animation graphs; example: elder and merchant conversations as graphs. |
| S15 Agent chat & vision | S14 | Planned | Follow-ups, selection/cursor context, `look_at_scene` with a local vision model, tools for S6-S14; example: "put a torch here", "spread the market out". |
| S16 Recipes & scatter | S15, S12 | Planned | One-click recipes, scatter tool; example: "Market square", "scatter 20 trees". |
| S17 Code agents | S16 | Planned | `create_component` scaffold, MCP resources/prompts, `UmbraTool agent-eval`; example: Claude Code adds a "bob" component; model eval report. |
| P11 Mobile | P10; approved device target | **Implemented 2026-11-08** (user decisions: Android, emulator first, touch + gamepad, landscape; `#UMBRA-79`): `plugins/android-export` Gradle app, `libmain.so`, `game.pack`, lifecycle (save, release/restore the Vulkan surface), touch controls and gamepad sticks; verified headlessly (T82); the Gradle/NDK build and the emulator run on Windows open, phones later | ADR 0009; `apps/umbra/Mobile.md`; evidence `testing/p11-android.md`. |
| F1 Community improvement | Separate product approval | Deferred proposal | Consent, records, validation, developer retraining, approved model delivery/rollback. |

No calendar schedule is asserted. Weekly capacity and actual throughput are unknown. Estimate a slice only after repository review; keep the dependency ordering and exit evidence even when estimates change.

### Progress conventions

Use **planned**, **in progress**, **implemented/unverified**, **verified complete**, **blocked**, or **deferred**. Preserve **documented complete** for inherited status. A blocked test is not a failed implementation and is not a passed gate. Evidence belongs in the repository's existing system: milestone summaries in `docs/current-engine-status.md`, verification records in `docs/testing/` (one file per gate or check, as with the Phase 1/2 smoke tests). Use the section 14 templates for their structure. This dashboard holds status only; do not maintain contradictory competing dashboards.

When a milestone is verified, include revision/dirty state, dated run, configuration/platform, automated results, manual checks and evidence path. Track test cases as not run until evidence is added; this roadmap initializes no test as newly passed.

## 1. Product charter and scope

Build a learning-first C++20 full-3D RPG engine, PC editor and complete single-player reference RPG. Make ordinary supported content data-driven. Make native systems small and inspectable enough for individual creators and coding agents to extend, test and rebuild.

Reference content: one settlement, one dungeon, one player character, one NPC, one enemy, one conversation, one skill tree, one quest, turn-based combat, and real-time fighting/exploration. Reference systems: movement, quest, save/load, stats, conversation, inventory/equipment, skills, both combat modes, and NPC/enemy AI.

**Independent modes are a product requirement.** Turn-based and real-time combat may share state, effects and definitions, but a game may choose either alone. The reference game's integration demonstrates interoperability, not compulsory coupling.

Initial AI delivery consists of AI-assisted quest/dialogue authoring plus a learned **turn-based** enemy policy. Developers train; players infer locally. The enemy's low-player-HP aggression is a statistical behavioral preference, not a mandatory action-selection rule. Conventional real-time AI is sufficient initially.

No network is required for installed gameplay. Keep saves local. Local authoring/training is preferred; any cloud provider is optional and explicit. Do not make an account, first-run model download, telemetry service or remote entitlement check necessary for the declared offline package.

**Scope exclusions:** multiplayer, modding/public plugins, runtime binary ABI support, mandatory C#/Lua/Python gameplay, arbitrary native hot reload, a browser editor, runtime LLM conversation as a requirement, on-player-device training, community services, custom production physics, seamless open world, advanced render features without a reference-game need.

Source-level C++ extension remains supported without a public plugin framework. Python, if selected for training, belongs only to developer tooling. Features excluded now may be proposed later with a concrete need and a cost/risk assessment.

## 2. Observable definition of done

### 2.1 Earlier checkpoint: complete game without learned AI

From a clean offline installation, a player can start in the settlement, move and interact, speak to the NPC, accept the quest, acquire/equip an item, unlock a skill-tree node, enter the dungeon, exercise real-time fighting, enter a separate turn-based encounter, resolve the quest/reward, and save/exit/reload. Death/retry, cancellation, repeated interaction and loading an earlier save have defined behavior.

This is a proposed walkthrough connecting confirmed features, not a prescribed story, camera or art direction. One enemy definition may be reused across encounters as a provisional content interpretation. A conventional-AI game is an earlier milestone, not a substitute for P8A/P8B.

### 2.2 PC engine release

The complete journey above works, the packaged learned turn-based policy runs offline, and missing/invalid policy behavior uses a documented conventional fallback. Both combat modes also pass standalone fixtures without initializing the other controller. Content authoring demonstrates a proposed quest/dialogue patch, validation, a visible diff, approval, undo, and replay of resulting content. The authoring model is not a runtime game dependency.

A creator other than the implementer can follow documented steps to build or use the editor, edit supported data, diagnose an intentional invalid reference, play, and package a game. Release artifacts exclude editor/training dependencies. Performance is measured on named hardware/settings; no benchmark on an empty scene is treated as proof of the game's 60 FPS goal.

### 2.3 Later platform gates

Web export means a playable browser game, not a browser editor. Offline readiness is declared only after required application/assets/model are cached successfully. Storage loss and explicit save export/import are handled. Browser capabilities are detected, documented and tested rather than promised universally. Mobile requires a separately selected device/OS profile and lifecycle/thermal testing.

## 3. Architecture and ownership

### 3.1 Dependency direction

```text
PC Editor / Game executable / Headless test-training runner
                              |
                  RPG definitions and simulation
        commands → legality → commitment → effects → events
                     /                   \
          Turn-based controller     Real-time controller
              (optional)               (optional)
                              |
                World / Assets / Physics interface
                              |
                  Core / Diagnostics / Platform

Renderer ← extracted presentation state, never authoritative rules
Inference ← immutable observations; returns action proposals
Authoring AI ← selected content/schema; returns proposed data patches
Training tools → reset/step of the actual native combat simulation
Later services ← bounded exported records; not a core dependency
```

Simulation/headless tests must run without a window, Vulkan device, ImGui context, a model, Python, account or network. Physics may be included for fixtures that need it. Do not implement a simplified training-only damage formula and call it the same environment.

Prefer explicit composition and direct interfaces over a general service locator or dynamic plugin registry. Initially, a modest RPG compilation target can contain separate controller implementations; selective source registration/build options may exclude either controller. Do not force a micro-library per class. The independence acceptance gate concerns dependencies and behavior, not a mandatory directory count.

### 3.2 Repository evolution

Retain existing `/apps/umbra` (target `Umbra`), Core, Diagnostics, Platform and Render. Phase 3 added `Umbra::World`, `Umbra::Assets` (`/engine/assets`), `Umbra::Scene` (`/engine/scene`) and `/tools/scenecheck` (`UmbraSceneCheck`). `/apps/editor`, `/apps/game`, `/engine/physics`, `/engine/script`, `/engine/editor` and `/samples` are still empty folders, not targets. Add only targets needed by the active slice. Names below are illustrative proposals; fit them to actual conventions before implementation.

```text
/engine/assets, world, physics           added as used
/gameplay/rpg/definitions, state, rules
/gameplay/rpg/stats, inventory, skills
/gameplay/rpg/combat/turn_based
/gameplay/rpg/combat/real_time
/gameplay/rpg/quests, conversation, ai
/engine/ai_inference                     optional, later
/apps/editor                            separate desktop executable, later
/apps/headless                          or equivalent test runner
/tools/assetc                           derived assets/validation as needed
/tools/train                            developer-only optional environment
/tools/authoring_ai                      authoring bridge, not gameplay
/tests                                  existing framework, extended incrementally
```

The folder tree is not a claim that these modules exist. Do not create empty targets or rename working components just to match the tree.

### 3.3 Core data contracts

| Contract | Required design property | First gate |
|---|---|---|
| Entity/resource identity | Generational runtime handle detects stale references; explicit ownership of creation/destruction | P3 |
| Durable identity | Stable definition/instance IDs in scenes/saves; reconstruct transient handles on load | P3/P6 |
| World lifecycle | A session/world generation invalidates pending commands/results after load/reset | P3/P6 |
| Definition versus state | Skill definition is not cooldown; item definition is not equipped instance; quest definition is not progress | P3/P5 |
| Source/derived asset pair | Schema/importer version, source/dependency hash, settings and target profile determine a built artifact | P3 |
| Validation | Parse, schema, reference, semantic and capability checks report structured errors | P3/P7 |
| Reload transaction | Stage new graph, validate/build it, publish at a safe point, retain old resources until no consumer uses them | P3 |
| Render extraction | Read-only presentation data and resource handles; no Vulkan object in gameplay state | P3 |
| RPG action | Actor/target IDs, stable skill/action ID, parameters, intent ID; authoritative revalidation before commit | P5 |
| Persisted snapshot | Versioned committed state, content versions and RNG information where relevant; no pointers/workers/GPU objects | P6 |
| Policy package | Model plus observation/normalization/action/ruleset metadata and evaluation evidence | P8A |

Do not serialize raw C++ struct memory as the file format. Choose explicit encodings, bounds, versioning and error paths. Avoid exposing third-party SDK types across unrelated engine boundaries.

### 3.4 Reload and ownership policy

Immutable authored definitions may be replaced transactionally. Existing mutable instances follow an explicit compatibility/migration policy. For active quests, skill activations or encounters, either pin a compatible definition revision until a safe boundary or migrate under a tested rule. Never mix half of an old dependency graph with half of a new one.

GPU resources cannot be destroyed merely because the CPU swapped an asset handle. Keep lifetime tied to completed GPU use. CPU readers, queued render work and model requests have similarly explicit ownership. On an incompatible edit, keep the last valid version and report what requires a restart.

Reference IDs cannot silently resolve to a different asset after a rename or case-only path change. Validate missing, duplicate and cyclic references. Treat oversized, malformed and path-traversing external assets as invalid input, not assertions in shipping code.

### 3.5 Minimum metadata, not a universal reflection language

Provide the small schema/descriptor capability needed for serialization, validation, inspection and editor fields. Reuse it for AI tool schemas where appropriate. Hand-written descriptors are an acceptable first implementation. A general C++ parser/code generator, visual programming language or cross-language binding framework is not required.

Keep human-readable source content; derived binary data is appropriate where loading or GPU preparation benefits. Serialization-library and source-schema choices are milestone decisions, not a requirement to adopt multiple formats immediately.

## 4. Shared RPG simulation

### 4.1 Commands and authoritative state

```text
Player / conventional enemy / learned policy
                     |
                 ActionIntent
                     |
        Validate actor, target, skill, cost,
          ownership, mode, range and readiness
                     |
              Commit transaction
                     |
            Resolve shared effects
                     |
      Committed events → quest/UI/audio/animation
```

Rejected actions cannot spend resources, grant rewards, advance turns or partially mutate equipment. Validation must be repeated when an asynchronous decision is committed. Side effects are identified so repeated delivery cannot duplicate damage or a quest reward.

Use fixed, documented event-resolution order for simultaneous effects/death in supported fixtures. Cross-platform bitwise determinism is not promised; record meaningful committed inputs/actions/outcomes for replay. Hidden perception facts must not leak to the learned policy through convenient access to global state.

### 4.2 Two controllers, not two combat engines

Common rules own stats, resources, modifiers, skill definitions, inventory/equipment, effects, targeting validation, death and reward semantics. The turn controller owns turn order, turn ownership, action/AP consumption and turn-boundary effects. The real-time controller owns wind-up, activation, hit windows, recovery, interruption and time-based readiness.

Mode-specific metadata is allowed. A turn duration and a duration in seconds are distinct types/domains; no undocumented numeric conversion. An incompatible skill can be rejected by the target mode's content validator. Not every skill must automatically work in both modes, but common skills should demonstrate shared effect resolution.

A turn-only fixture must not initialize the real-time controller or require animation time to advance a turn. A real-time-only fixture must not construct turn state or submit artificial turns. The combined game composes them through a small encounter coordinator at explicit boundaries.

### 4.3 Reference integration contract

Proposed state flow:

```text
Exploration → RealTimeEncounter → Resolution → Exploration
Exploration → EncounterPreparation → TurnBasedEncounter → Resolution → Exploration
```

Before changing mode: stop accepting outgoing-mode decisions; finish/cancel active actions under the selected cost/refund rule; invalidate old policy requests; apply the documented status/cooldown policy; bind shared character state to the new controller. The two controllers never commit effects concurrently for the same encounter.

Arbitrary mid-swing or mid-projectile conversion is excluded initially. This is a scope default, not a universal engine limitation. Decide whether each persistent effect pauses, expires, or uses explicit per-mode semantics before adding it to cross-mode content.

### 4.4 System contracts

| System | Minimum implementation | Invariants and observable failures |
|---|---|---|
| Movement | Input-to-movement command, collision, camera connection, interaction range | Authoritative pose is not renderer-owned; focus loss cannot leave movement stuck. |
| Stats | Base/derived stats, bounded resources, deterministic modifier ordering within a fixture | Equip/unequip cannot produce invalid maxima, NaNs or double-applied bonuses. |
| Inventory/equipment | Item instances, only needed stacking/slots, add/remove/equip transactions | Reject or explicitly handle full capacity; no item duplication or disappearance on partial failure. |
| Skills/tree | Validated prerequisites, skill-point spending, effects/costs, small progression tree | No cycle, duplicate spend, stale prerequisite or unsupported-mode silent success. |
| Turn-based | Turn order, ownership, legal actions, resolution, waits, terminal states | One action consumes the intended opportunity once; no infinite no-legal-action turn. |
| Real-time | Attack lifecycle, collision/hit query, cooldown, interruption | Duplicate callbacks cannot create unintended hits; cost/refund timing is explicit. |
| NPC/enemy AI | Conventional baseline; legal observations; pluggable source-level decision interface | All decisions use ordinary action validation; fallback is observable and separate from learned behavior. |
| Conversation | Nodes, choices, conditions, exit and interruption | Invalid/missing links report errors; user can leave a failed interaction safely. |
| Quest | State/objectives, event conditions, completion/failure policy, reward transaction | Early/repeated events have defined handling; reward commits once. |
| Save/load | Safe-boundary snapshots, version checks, migration, backup, atomic replacement where supported | Failure preserves a valid prior save and avoids a partially loaded live world. |

Audio, animation, text/HUD, pause/menu, camera and feedback are supporting features, not a second product. Add only what the reference game uses. Minimal character animation may require a controlled skeletal or rigid-animation path; select the simplest honest representation rather than pretending an existing rotating model is an RPG animation system.

### 4.5 Save, checkpoint and reset are related but different

Player saves represent product behavior. Development checkpoints accelerate iteration. Training resets create controlled episodes. Reuse serializers/state helpers when appropriate, but define the additional state each operation must restore or discard.

Persist committed gameplay state and compatible content/model identifiers. Do not persist raw pending inference jobs. Invalidate outstanding workers on load/reset; reconstruct requests only after the new world is valid. Do not regenerate past quest rewards or already committed AI decisions during load.

Test interrupted writes, read-only/full storage, corruption, newer/older schemas and missing definitions. Use a temporary write plus validated replacement and backup policy; validate platform-specific durability behavior rather than promising that any file rename is sufficient. On migration failure, preserve the original bytes and leave the active world unchanged.

A training reset also resets event queues, cooldowns, subscriptions, RNG streams, terminal flags and policy state when applicable. A surviving hidden counter can invalidate training results despite a visually reset scene.

## 5. Learned enemy policies

### 5.1 Initial scope and contract

Build a small turn-based policy over a fixed, versioned action catalogue. It chooses high-level skills/tactics, not rendered animation frames. First implementation may be feed-forward; recurrent memory, multi-agent training, continuous controls and real-time learned combat are separate experiments.

| Contract field | Initial proposal |
|---|---|
| Observations | Own/player HP fractions; own resources; allowed visible status; skill readiness; relevant turn context; range only when it affects this encounter |
| Actions | Basic attack, a small number of named skills, defend, wait; add reposition only if the actual turn rules support it |
| Legality | Same rules as gameplay; ordered mask aligned to stable action catalogue |
| Objective | Combat competence plus bounded style preference; difficulty/readability constraints evaluated separately |
| Reset | Seeded scenario and configuration; clear all episode-owned state |
| End | True combat terminal result distinct from an external time/step limit |
| Output | Action/logits/probabilities according to a fixed exported contract; never executable source |

Expose only information a designer permits the enemy to know. Do not feed future player actions, hidden inventory or unobservable cooldowns merely because they exist in memory. Adding arbitrary skills later does not teach an existing model their meaning; require mapping/compatibility validation and usually retraining or reevaluation.

Gymnasium is a reference for reset/step and terminated/truncated semantics [S2]. MaskablePPO is a candidate discrete-action algorithm, with mask-aware evaluation requirements [S3]. These references inform a spike, not a mandatory framework adoption.

### 5.2 Low-player-HP aggression is a learned preference

The user selected a preference, so do **not** implement `if player_hp < threshold then force_attack` as the learned behavior. Legality masks remove impossible/forbidden actions, not merely less aggressive strategies. A lawful defensive choice is allowed even at low player HP.

Proposed training approach: expose permitted player-HP information; include a bounded style signal or suitable examples/scenario distribution; maintain a primary combat objective; evaluate on scenarios not used for training/tuning. Exact reward weights are an experiment. Do not call an invented number a validated setting.

Define “aggression” in the test fixture using designer-labeled action characteristics, such as offensive commitment versus defend/wait, not just enemy win rate. For paired high/low-player-HP probes, hold other state and legal actions fixed where possible. Stratify confounders such as lethal opportunities, resource scarcity and survival emergencies; a simple low-HP attack-rate increase alone does not establish a general tactical preference.

Compare the policy with the conventional baseline and multiple training seeds. For stochastic policies, sample the same held-out probes repeatedly or inspect the exported action distribution. For deterministic deployment, measure selected actions over the probe set. Record distributions, uncertainty and exceptions. Set acceptance margins before final evaluation, using feasibility results rather than post-hoc cherry-picking.

The target is a contextually sensible tendency: more offensive pressure in appropriate low-player-HP situations without forcing suicidal, illegal or repetitive play. Test counterexamples where defense is necessary. Track fallback decisions separately so a fallback cannot manufacture apparent model success.

Reward shaping can introduce exploits. Test repeated defense/heal loops, prolonging combat for bonus rewards, intentional damage-taking and repeated no-op rewards. Behavior quality, intended difficulty and readability matter in addition to training reward or victory rate. These are design/evaluation requirements, not a claim any algorithm guarantees them.

### 5.3 Reuse the real environment

The native turn simulation owns costs, targets, damage, status timing, death and legal actions. A trainer adapter exposes a reset and an enemy-decision step. Define how the fixed/scripted opponent acts between enemy decisions and how rewards accumulate across those intervening actions. Do not advance one turn in Python and a different number in C++ deployment.

Specify whether one trainer step equals an enemy decision opportunity, a whole round or another unit; record discounting and episode-length interpretation consistently. When the encounter ends before the next enemy turn, return terminal state rather than fabricating another valid decision. Distinguish termination from step-limit truncation [S2].

Use varied initial stats/resources and multiple simple opponent strategies. Separate training, tuning and held-out evaluation scenario sets. Preserve seeds/configuration/checkpoints/versions; do not require byte-identical GPU training runs to establish reproducible methodology.

### 5.4 Training-to-runtime pipeline

1. Implement conventional enemy baseline and record legal behavior/outcomes.
2. Prove the headless native simulation/reset tests and fixed-observation/action contract.
3. Select a pinned developer-only training toolchain through a small experiment. External Python is a proposed default, not a gameplay runtime requirement.
4. Train against approved scenario distributions and objective definitions.
5. Export policy inference and required preprocessing/metadata. Do not assume a training checkpoint itself is deployable.
6. Compare training execution, exported model and native execution on representative, extreme and invalid observations/masks. Define numeric tolerances and deterministic tie handling where needed.
7. Evaluate complete encounters through the native runtime and the actual masking/action-selection path. Test difficulty/style, safety, resources and fallback behavior.
8. Package only an accepted model with a conventional fallback and documented provenance.

ONNX Runtime provides a C++ deployment path [S4]; it is the initial integration candidate. A smaller custom policy evaluator is not forbidden, but requires measured justification and equivalent export/validation tests. Do not add both runtimes without a need.

If masking is performed outside the exported network, use the same order, logit rules and selection behavior during evaluation/deployment. All-invalid masks, NaNs, malformed tensor shapes, out-of-range action indices and incompatible metadata must select a defined safe fallback or produce a controlled encounter error.

### 5.5 Versioned policy package

Proposed manifest fields: model ID and hash; model format/opset; policy version; observation field order and shapes; preprocessing/normalization version and ranges; ordered action IDs; relevant skill/ruleset hash; permitted mode; architecture/runtime/operator requirements; difficulty/style identity; training configuration reference; evaluation report; license/provenance; resource observations.

Content changes can invalidate semantics without changing tensor shapes. A reordered skill list or adjusted cost is not automatically compatible. Validate before encounter start and pin the accepted policy/version for that encounter. Model loading should have size/operator/resource limits. ONNX format validation does not prove safety or fitness; its own guidance warns about excessive-resource models [S7].

### 5.6 Interactive scheduling and failure behavior

Inference takes immutable snapshots and returns proposals. Tag each request/result with session/world generation, encounter ID, actor handle generation, decision sequence, simulation tick and policy version. Commit only if ownership and relevant state are still valid. Death, load, reset or mode change invalidates old results.

Use bounded work queues, a bounded worker budget and one in-flight decision per actor/turn where appropriate. A decision deadline triggers conventional behavior; late results cannot later override the committed fallback. If work is still running, stop issuing replacement work that would cause an unbounded backlog. A timeout is not proof that a native inference operation has been terminated. Define cancellation support, shutdown/join policy and provider disablement; use process isolation only if evidence warrants it.

Start by benchmarking CPU inference for the small policy. No inference per rendered frame is required. A worker still competes for shared hardware; control its thread count and measure whole-game frame spikes. ONNX Runtime exposes intra/inter-operation threading controls [S5]. Headless training may synchronously wait at a decision boundary because no interactive rendering is blocked.

## 6. AI-assisted authoring

Keep quest/dialogue generation in editor/tools. Its result becomes ordinary validated content; the shipped game does not need the generator or prompt history. Select one local provider or bridge first. `llama.cpp` is one C/C++ local-inference candidate [S12], not a dependency to add now. An explicitly configured cloud backup is optional; local failure never silently sends project data online.

Proposed transaction:

```text
Selected content + user intent + permitted schema/context
  → bounded generation request
  → proposed structured patch with base revision
  → parse/schema/reference/rule validation
  → human-readable diff + diagnostics
  → user approval
  → atomic commit + undo history
  → scenario tests/playthrough
```

Constrain target files/definitions and patch size. Compare the expected base revision before applying, so an old generated patch cannot overwrite newer user edits. Treat source text, retrieved lore and model output as untrusted data; they cannot authorize shell execution, file deletion, permission changes or secret access.

Validate dialogue exit paths, IDs, missing actors/items, reward cycles, objective references, localization references, invalid prerequisites and references to unavailable content. Structural validation cannot prove that every quest is solvable or enjoyable. Use scenario fixtures and human review for narrative/solvability judgments; never label generated content production-ready solely because it parses.

The first UX needs only intent input, selected context, progress/cancel, preview/diff, actionable validation errors, approval and undo. Full visual scripting, a generic autonomous agent platform and automatic code execution are excluded. Reuse normal editor commands and validators rather than an alternate AI-only mutation path.

## 7. Optional community improvement

This remains a **post-release proposal requiring explicit scope approval**. It must not hold up the offline engine or install an uploader in anticipation of later use.

Fixed player-side weights imply that the compatible initial interpretation of “local enemy knowledge” is experience records rather than locally updated weights. Proposed future flow: opt-in bounded encounter records → validated version-aware dataset → reproduce failures/enrich scenarios → developer training/evaluation → approved model release → optional download/activation at a safe boundary.

This is not a commitment to federated learning or client-side training. Nor is an upload assumed to directly improve a model. For example, SB3's on-policy workflow collects experience using its current policy [S11]; arbitrary historical records require an appropriate learning method. Initially, use contributions to discover failures and improve evaluation/scenario coverage.

Later requirements would include separate collection/upload/update consent; no raw save or unrelated personal data by default; bounded retention and deletion; malformed/duplicate/poisoned-data handling; model/ruleset/difficulty identities; staged evaluation; delivered-model integrity/authenticity checks; rollout/rollback; no mid-encounter replacement; continued play with the installed offline model. A model hash detects identity differences but is not by itself publisher authentication.

Do not claim each update improves every player's experience. Maintain difficulty/readability profiles and evaluate failures as well as wins. Budget and operational responsibility must be approved before implementation.

## 8. Milestones and work packages

Every package below ends in a runnable artifact or a bounded decision experiment. Task IDs are stable handoff identifiers; splitting a task adds sub-IDs rather than renumbering unrelated work. All are planned unless the dashboard/evidence says otherwise.

### B0 — Reconcile the actual repository

**Purpose:** avoid implementing from a stale September snapshot or treating proposed paths as real targets.

Inspect current source control state, applicable instructions, target graph, presets, manifests, existing status/testing records, and changes since the source baseline. Identify the oldest unmet gate. Run the documented baseline checks supported by the environment; record unavailable GUI/GPU/toolchain checks as not run. Confirm whether CI and its configurations actually exist rather than recreating them from the plan.

**Artifact:** a short reconciliation/evidence entry with current revision/dirty state, observed modules, inherited versus newly checked milestones, environment, actual commands, and next task. No renderer rewrite, dependency upgrade or folder reorganization is part of this audit.

**Exit:** source/docs discrepancies are visible and the next slice has a bounded acceptance test. If the checkout already completed Phase 3, select its next genuine gap instead of restarting it.

**Findings so far (2026-11-01, partial — full record in `testing/b0-baseline-reconciliation.md`):**

| # | Finding | Action |
|---|---|---|
| 1 | HEAD `63e6fe7` (2026-09-02) on `main`. Uncommitted: `engine/world/` (empty `World` class + `Umbra::World` target), wiring in root and `apps/umbra` CMake. | Used as the P3.1 starting point and committed with it (`#UMBRA-13`). |
| 2 | Phase 1 is "complete" in the status doc, but `testing/phase1-smoke-test.md` is "Partial" with no ticked items. | User sign-off needed, or record the gap. |
| 3 | Registered tests: `Umbra.Core`, `Umbra.Diagnostics.*` (formatting + fatal/assert/terminate crash probes, plus `windows-exception` on Windows), `Umbra.Platform`, `Umbra.Render`. The older text listing only `UmbraCoreTests` is stale. | Use the real list. |
| 4 | `.github/workflows/windows.yml` sets `VCPKG_ROOT: E:\Projects\vcpkg`, a local path that `windows-latest` runners don't have. Hosted CI probably can't configure. | Check real CI runs before relying on CI evidence. |
| 5 | Small duplication: `umbra_copy_asan_runtime` is defined twice in `cmake/UmbraSanitizers.cmake`; `engine/core/CMakeLists.txt` repeats the same `target_link_libraries` block. | Fixed in P3.1 (core) and P3.8 (sanitizers). |
| 6 | Core/Diagnostics/Platform build with zero warnings (`-Wall -Wextra -Wpedantic`) and all 6 CTest entries pass on Linux GCC 13 with SDL3 3.2.24 (dummy video driver). | Portability evidence only. Not a Windows gate. |
| 7 | Not run: any Windows preset, `Umbra.Render`, the app, Vulkan validation, RenderDoc. | Still open B0 items. |

### P3 — Assets and World

<!-- P3-status -->
| Task | Status | Evidence |
|---|---|---|
| P3.1 | Implemented; headless tests pass (Linux GCC/Clang, GCC ASan+UBSan); Windows not run | `tests/core/SlotMapTests.cpp`, `tests/world/WorldTests.cpp` |
| P3.2 | Implemented; headless tests pass (Linux GCC/Clang, GCC ASan+UBSan); Windows not run | `tests/world/TransformTests.cpp`; conventions in `engine/world/include/Transform.md` |
| P3.3 | Implemented/unverified: world extraction tests pass; Render + app compile on Linux GCC/Clang; Scene.hlsl checked with glslang (not DXC); GPU run, Vulkan validation and MSVC not run | `tests/world/ExtractionTests.cpp`; push-constant draw path in `VulkanFrameResources.cpp` |
| P3.4 | Implemented/unverified: RetireQueue + mesh/texture validation tests pass; Render create/replace/destroy + fallback texture compile on Linux; GPU run, validation layers and MSVC not run | `tests/core/ResourceDataTests.cpp`; design in `engine/render/src/ResourceLifetime.md` |
| P3.5 | Implemented: importer + cache tests pass (Linux GCC/Clang, GCC ASan+UBSan); app loads models through the cache (compile-checked only). Windows/vcpkg tinygltf 3.0 build not run | `tests/assets/*`, `tests/scene/ModelInstanceTests.cpp`; format in `engine/assets/include/AssetPipeline.md` |
| P3.6 | Implemented: scene load/save/validate tests pass (Linux GCC/Clang, GCC ASan+UBSan); UmbraSceneCheck CLI + CTest on sample scene; app loads scenes into a fresh World (compile-checked only) | `tests/scene/SceneFileTests.cpp`, `Umbra.SceneCheck.Sample`; format in `engine/scene/include/SceneFormat.md` |
| P3.7 | Implemented/unverified: FileWatcher + SameStructure tests pass (Linux GCC/Clang, GCC ASan+UBSan); transactional model/scene reload in the app compile-checked only; live edit-and-observe NOT run | `tests/assets/ReloadTests.cpp`; protocol in `engine/assets/include/Reload.md` |
| P3.8 | Implemented: Umbra.HeadlessDependencies (verified to catch an injected leak), validator CTests on good/broken scenes, --json diagnostics; evidence record written. Windows gate open | `cmake/UmbraHeadlessCheck.cmake`, `docs/testing/phase3-assets-world.md` |
<!-- /P3-status -->

**Prerequisite:** reconciled B0; existing rendering retained. **Goal:** replace the hard-coded primitive with a general small world and dependable asset identities/lifetimes.

| Task | Implement | Required evidence |
|---|---|---|
| P3.1 World identity and ownership | World lifetime; generational entity handles; durable authored IDs; create/destroy; explicit component ownership using a simple storage model | Headless create/destroy/reuse test; stale handle rejected; multiple worlds/reset do not alias identities |
| P3.2 Transform hierarchy | Local/world transform policy, parent ownership, dirty propagation, deletion/reparent policy | Parent/child composition tests; cycle rejection; deleted parent safe behavior; coordinate conventions recorded |
| P3.3 Render extraction | Multiple renderable entities → draw list using engine-owned handles; integrate with existing scene pipeline | Two or more independently transformed objects render; gameplay does not include Vulkan headers |
| P3.4 Resource lifecycle | Generational mesh/texture identities, explicit load/failure/ready states, dependency ownership and deferred destruction | Failed/missing resource path; no stale draw/use-after-free during replacement; validation-clean presentation |
| P3.5 Import and derived assets | Preserve glTF scene/node hierarchy, meshes/material references needed by the scene; versioned import output, source hashes/settings, dependency index | Rebuild invalidated dependencies only; unsupported asset feature diagnosed; repeated build has stable output under declared conditions |
| P3.6 Scene persistence and validation | Versioned source scene format, durable references, bounds/reference checks; headless validator | Save/reload scene state; malformed/missing/duplicate/cyclic input rejected without partial live-world mutation |
| P3.7 Transactional asset reload | File/change detection, stage/validate/build, safe publication, old-resource retirement | Edit source asset and observe coherent reload; incomplete write retains last good version; GPU use remains valid |
| P3.8 Headless and evidence gate | Consolidate scene/world fixtures, target-dependency checks, machine-readable diagnostic fields | Run without window/GPU/editor/model; update actual status and capture required graphical checks |

**Exit gate:** a multi-object scene with preserved hierarchy loads and renders; source edit safely updates derived content; stale references/failure cases are controlled; world tests require no renderer/editor. Match T01–T04, T43–T44, T50–T51, T53 and T57 as applicable.

**Do not add:** ML runtimes, Python, a universal ECS, general scripting, a plugin ABI, jobs/allocators without evidence, or an elaborate editor. A small diagnostics panel is acceptable when it helps verify ownership.

### W0 — Early web feasibility experiment

**Dependency:** stable P3 core interfaces. **Purpose:** reduce port risk before more platform-specific code accumulates; not a full browser port.

Prove a small native-independent simulation fixture builds to WebAssembly. Render a representative simple scene using a browser-compatible graphics path. Write/read a persistent test value with explicit error handling. Later, when a candidate model exists, run one observation through the browser inference adapter. Record shader format/conversion, deployment headers, memory limits, lifecycle and worker constraints that affect architecture.

Emscripten documents a WebGPU integration through Emdawnwebgpu [S8]. Investigate it as a candidate rather than treating Vulkan calls or existing SPIR-V pipelines as browser-ready. Decide how HLSL sources reach the selected browser shader path and validate the actual outputs. Preserve the working Vulkan backend.

**Exit artifact:** a minimal runnable browser sample and an ADR identifying tested versions/browsers, chosen or rejected backend path, dependencies, limitations and revisit triggers. Failure is a useful result if recorded accurately and converted to an explicit port risk. Do not block all PC work while turning the experiment into a general RHI framework.

### P4 — Playable 3D sandbox

**Dependency:** P3. **Goal:** a controllable, inspectable 3D space rather than a renderer demo.

| Task | Implement | Required evidence |
|---|---|---|
| P4.1 Physics adapter spike | Evaluate original Jolt direction behind an engine-owned interface; bodies/shapes/queries/layers, deterministic ownership and debug draw | Small scene with collision/query tests; unsupported/deferred behavior documented; no Jolt objects in gameplay APIs |
| P4.2 Player movement/camera | Select reversible camera/control defaults; collision-aware movement and interaction query; focus/pause behavior | Move through settlement/dungeon test geometry; no stuck input; blocked movement and missing focus handled |
| P4.3 Presentation essentials | Minimal character/action animation, sound playback, readable text/HUD, interaction feedback, pause/retry UI; controlled formats only | State/action feedback visible/audible; pause/resume and resource cleanup tests; missing asset diagnostics |
| P4.4 Scene transition/checkpoint | Small-area load/unload; development checkpoint/reset path over established state helpers | Repeated entry/exit/restart without stale entities/resources or unbounded memory growth |
| P4.5 Observability | CPU frame/simulation timings, memory/resource counters, existing capture/validation workflow | Baseline measurements on available machine; not a final 60 FPS certification |

<!-- P4-status -->
| Task | Status | Evidence |
|---|---|---|
| P4.1 | Implemented: Jolt adapter, collider component + scene field, physics bridge, fixed 60 Hz stepping, debug outlines (F1); tests pass on Linux GCC/Clang/ASan; Windows not run | `tests/physics/PhysicsTests.cpp`, `tests/gameplay/PhysicsBridgeTests.cpp`; `engine/physics/include/Physics.md` |
| P4.2 | Implemented: third-person PlayerController (camera-relative movement, sprint, jump, turn, wall-aware orbit camera, interaction ray), mouse input + capture, camera moved out of the renderer, settlement test scene; walk-through tests pass on Linux GCC/Clang/ASan; Windows/interactive not run | `tests/gameplay/PlayerControllerTests.cpp`, `tests/gameplay/SettlementWalkTests.cpp`, `tests/platform/InputTests.cpp`; `game/gameplay/include/PlayerController.md` |
| P4.3 | Implemented: procedural walk bob/lean + footstep timing (`StrideAnimator`), WAV loader/mixer (`Umbra::Audio`, 48 kHz 16-bit PCM only) + SDL audio device, generated placeholder sounds, interactables (`interactable` scene field, E key, prompt/message HUD), Esc pause menu (resume/restart/quit, game audio paused, UI audio not), missing-asset warnings on screen and in `UmbraSceneCheck`; tests pass on Linux GCC/Clang/ASan; Windows/interactive/audible not checked | `tests/audio/AudioTests.cpp`, `tests/platform/AudioDeviceTests.cpp`, `tests/gameplay/PresentationTests.cpp`, `tests/gameplay/SettlementWalkTests.cpp`, `tests/scene/SceneFileTests.cpp`; `engine/audio/include/Audio.md`, `game/gameplay/include/StrideAnimator.md` |
| P4.4 | Implemented: `portal` scene field + spawn markers, settlement ↔ dungeon areas (each load gets a fresh world and physics world), development checkpoint F5/F9 over `SaveScene`/`LoadScene`, R restart; repeated trips keep body/character/entity counts constant and checkpoint round trips are byte-identical; LSan (GCC ASan build) clean; Windows/interactive not run | `tests/gameplay/AreaTests.cpp`, `tests/scene/SceneFileTests.cpp` (portal, checkpoint cases), `Umbra.SceneCheck.Dungeon`; `engine/scene/include/SceneFormat.md` |
| P4.5 | Implemented: CPU section timings (rolling avg/max) and live counters (physics steps, bodies, characters, entities, models, sounds, area) in the diagnostics overlay; hidden `[benchmark]` test; Linux smoke baseline recorded; Windows/GPU numbers pending | `tests/core/ProfilingTests.cpp`; [`testing/phase4-sandbox.md`](testing/phase4-sandbox.md) |
<!-- /P4-status -->

**Exit gate:** walk, collide, interact and restart in a small area; graphical/physics state lifetimes are inspectable. Verify T05, T44, T56 and relevant P3 regressions.

**Not required yet:** cinematic animation blending, complex navigation crowds, full PBR, open-world streaming or mobile packaging. Add lighting/animation quality only when the selected reference presentation needs it; record remaining original learning experiments as deferred, not silently completed.

### P5 — Shared RPG rules and two independent combat modes

**Dependency:** P3 for headless rules; P4 for visual integration. **Goal:** one authoritative rule implementation and separate controllers.

| Task | Implement | Required evidence |
|---|---|---|
| P5.1 Definitions/state | Actor/item/skill/effect schemas, stat bounds, derived modifiers, state ownership, errors | Invalid definitions rejected; modifiers recompute consistently; no render dependency |
| P5.2 Inventory/equipment | Minimal slots/instances/stacking, atomic acquire/remove/equip; overflow policy decision | Full inventory and invalid equip tests; no partial mutation or duplicate bonuses |
| P5.3 Skills and tree | Prerequisite validation, points/unlocks, costs, shared effect descriptions and mode capabilities | Cycle/missing prerequisite/repeated unlock rejection; shared effect fixture |
| P5.4 Action resolver | Intent validation, commitment/cost timing, effects, statuses, death, idempotent events | Player and conventional AI drive same legal path; repeated/invalid intents have no extra effects |
| P5.5 Turn controller | Chosen order/AP policy, waits, status ticks, terminal handling, conventional turn enemy | Complete headless turn encounter without real-time controller; fixed scenario result/regression |
| P5.6 Real-time controller | Wind-up/commit/hit/recovery, collision or target query, cancellation/cooldowns, conventional real-time enemy | Complete real-time encounter without turn state; duplicate hit/interruption tests |
| P5.7 Optional integration | Encounter coordinator, safe boundaries, shared progression, explicit clock/status transition rules | Combined reference fixture; one controller commits at a time; stale outgoing decisions rejected |
| P5.8 Headless training-ready seam | Reset/snapshot/step opportunities without introducing training framework | Repeated reset isolates state; equivalent rule outcomes between fixtures and game commands |

**Exit gate:** turn-only, real-time-only and combined fixtures all pass; shared damage/cost/death rules are not duplicated. Effects with unsupported clock semantics fail validation instead of being silently reinterpreted. Verify T06–T12, T39–T40, T46–T47, T49, T55 and T60.

**Important:** “independent” does not require dynamic loading or a plugin SDK. It requires that either mode can function without the other. Use the simplest compile/registration separation demonstrating this property.

<!-- P5-status -->
| Task | Status | Evidence |
|---|---|---|
| P5.1 | Implemented: `Umbra::Rpg`, versioned `umbra.rpg` JSON content (statuses with separate turn/second clocks, skills, items, actor archetypes), all-or-nothing validation, character state by ids/counts, derived stats | `tests/rpg/ContentTests.cpp`; `game/rpg/include/Rpg.md` |
| P5.2 | Implemented: all-or-nothing add/remove/equip/unequip; full inventory rejects the pickup (user decision); maxima clamp, never heal | `tests/rpg/InventoryTests.cpp` (T11, T46) |
| P5.3 | Implemented: unlock with prerequisites and points; requirement loops rejected on load | `tests/rpg/SkillTreeTests.cpp` (T12) |
| P5.4 | Implemented: shared resolver (check, pay, apply, deaths, events), decision-id log | `tests/rpg/RulesTests.cpp` (T07, T10, T46) |
| P5.5 | Implemented: `Umbra::TurnCombat`, speed rounds with one action (user decision), wait, status ticks, conventional enemy, fixed regression | `tests/turncombat/TurnEncounterTests.cpp`, `Umbra.TurnOnlyDependencies` (T39); `game/turncombat/include/TurnCombat.md` |
| P5.6 | Implemented: `Umbra::RealtimeCombat`, wind-up/commit/active/recovery, pay at commit (user decision), range+cone hits once per activation, interruption, cooldowns, conventional enemy | `tests/realtimecombat/RealtimeEncounterTests.cpp`, `Umbra.RealtimeOnlyDependencies` (T06, T08, T40); `game/realtimecombat/include/RealtimeCombat.md` |
| P5.7 | Implemented: `Umbra::Encounter`, one controller at a time, modes change only between fights, statuses/cooldowns end with the encounter (user decision), stale decisions rejected | `tests/encounter/EncounterTests.cpp` (T09, T47); `game/encounter/include/Encounter.md` |
| P5.8 | Implemented: `TurnEnvironment` reset / legal actions / step over the real turn controller; reset isolation and game-path parity | `tests/turncombat/TurnEnvironmentTests.cpp` (T49, T55, T60) |

### P6 — Complete offline reference RPG

**Dependency:** P4/P5. **Goal:** finish the game with conventional AI before AI experiments become release dependencies.

| Task | Implement | Required evidence |
|---|---|---|
| P6.1 Conversation | Node/choice/condition schema, interaction ownership, completion/cancellation/exit | One conversation works; repeated interaction and interrupted/missing node cases are safe |
| P6.2 Quest | Objective progression, early-event policy, completion/failure, idempotent rewards | One full quest; repeated/early events and full inventory behavior pass |
| P6.3 Save/load | Safe-boundary state snapshots, version/checksum or equivalent integrity checks, staged load, backup/migration policy | Save/exit/reload all required progression; corrupt/old/new/missing-content and disk-failure tests |
| P6.4 Reference content | Settlement/dungeon, one player/NPC/enemy content scope, equipment/skill unlock and separate encounters | Complete the reference walkthrough; content arrangement respects recorded assumptions |
| P6.5 Failure/retry/offline | Death/retry, pause, repeated transitions, clean shutdown, no network requirement | Play with network unavailable; retry/load do not duplicate rewards or retain pending work |

<!-- P6-status -->
| Task | Status | Evidence |
|---|---|---|
| P6.1 | Implemented: `Umbra::Story` conversations (conditions, all-or-nothing actions, walk away), quests/conversations in `umbra.rpg` | `tests/story/ConversationTests.cpp` (T15); `game/story/include/Story.md` |
| P6.2 | Implemented: ordered objectives, early events ignored, failure on death, reward once, XP and levels, overflow kept as pending | `tests/story/QuestTests.cpp`, `tests/rpg/ProgressionTests.cpp` (T13, T14) |
| P6.3 | Implemented: `umbra.save` v1 with checksum, write-verify-backup-rename, full validation, backup fallback | `tests/game/SaveGameTests.cpp` (T16–T18, T49); `game/game/include/SaveGame.md` |
| P6.4 | Implemented: `GameSession`, roamers and spawners, Elder Mara, save stones, three dungeon zones; app wiring in `apps/umbra/GameFlow`; fights in a separate arena scene after the first play test (`#UMBRA-43`) | `tests/game/GameSessionTests.cpp`, `tests/gameplay/RoamerTests.cpp`; `apps/umbra/GameFlow.md` |
| P6.5 | Implemented: death menu, load mid-fight ends it (T19), retry pays once, no network code | `tests/game/GameSessionTests.cpp`; play-through checklist open |
<!-- /P6-status -->

**Exit gate:** the full journey is playable from a clean package using conventional AI; state survives restart; failure does not destroy the last good save. Verify T13–T19, T43, T45, T49 and relevant system tests.

**Boundary:** do not hide incomplete gameplay behind a training UI. This milestone can complete while P8A research is still failing or not started.

### P7 — Creator workflow, validation and iteration

**Dependency:** schemas and game systems exist. Small editor/CLI work may begin earlier when it directly verifies an active milestone.

| Task | Implement | Required evidence |
|---|---|---|
| P7.1 Minimal PC editor | Hierarchy/selection, transform/property editing, asset browser, diagnostics; save/open scene | Create/edit supported content without manual source edits; runtime build excludes editor |
| P7.2 RPG content forms | Items/skills/tree, actor, conversation and quest forms or equivalent structured editing | Add a supported item/skill/quest via data only; no C++ rebuild for supported data changes |
| P7.3 Shared commands and undo | One mutation/validation path, transaction boundaries, undo/redo, safe failure | Redo/undo content patch; conflict with newer edit rejected or explicitly resolved |
| P7.4 Headless tool surface | Validate content, run scenario, inspect schema, build/package wrapper where useful; structured diagnostics | Same invalid input produces useful matching CLI/editor diagnostics; noninteractive test fixtures |
| P7.5 Iteration workflow | Data safe reload; targeted C++ rebuild; play restart and compatible checkpoint restore; preserved known-good build/content | Measured representative edits; failed compile/reload does not lose the known-good pairing |
| P7.6 Creator onboarding | Sample project/template, concise commands and troubleshooting, package operation | Fresh user follows the guide, changes data, diagnoses planted error, runs and packages the game |

<!-- P7-status -->
| Task | Status | Evidence |
|---|---|---|
| P7.1 | Implemented (compiled; not yet run): `apps/editor`, hierarchy, inspector, asset browser, diagnostics, ImGuizmo, picking, Play | `apps/editor/Editor.md`; `Umbra.GameExcludesEditor` |
| P7.2 | Implemented: RPG forms for items, skills, statuses, actors, quests, conversations | `apps/editor/Editor.md`; `DocumentTests` (item + quest by data only) |
| P7.3 | Implemented: `Umbra::Authoring` documents: validated all-or-nothing edits, undo/redo, merged drags, revision conflicts, atomic save with outside-change detection | `tests/authoring/DocumentTests.cpp` (T48); `engine/authoring/include/Authoring.md` |
| P7.4 | Implemented: `UmbraTool validate / inspect / schema / fight`, JSON diagnostics shared with the editor | `tests/authoring/ValidateTests.cpp`, `Umbra.Tool.*`; `tools/umbratool/README.md` |
| P7.5 | Implemented: live RPG content reload at safe moments (checked like a save), assets/shaders found next to the program, install rules, known-good build copy, measured rebuilds | `GameSessionTests` (reload check, T51); `apps/umbra/GameFlow.md`; measurements in the record |
| P7.6 | Implemented: `scripts/package.bat` (folder + zip), `scripts/editor.bat`, `docs/creator-guide.md` with a planted-error exercise | Second-person walkthrough open |
<!-- /P7-status -->

**Exit gate:** a second person performs the creator walkthrough without undocumented manual fixes. Single-creator usability takes priority over collaboration servers, visual scripting or a full asset marketplace. Verify T22, T37, T42, T48, T51, T53 and T58.

**CLI names remain proposals** until implemented. Do not document `umbra validate` or a similar command as working merely because it appears as a desired capability.

### P8A — Learned turn-based enemy, independent research track

**Dependency:** stable P5 turn rules, conventional baseline and native headless reset/step. Full visual editor is not required for the first experiment.

| Task | Implement | Required evidence |
|---|---|---|
| P8A.1 Environment contract | Observations, fixed action catalogue, masks, enemy-decision step/opponent behavior, reset/terminal/truncation | Contract fixtures; no hidden-state leakage; trainer step uses actual native resolver |
| P8A.2 Training spike | Select/pin developer-only tools; tiny baseline model and reproducible recipe | Train a bounded example; record environment, seeds, time/resources and failure modes; no engine dependency on Python |
| P8A.3 Preference objective/evaluation | Low-player-HP aggression as soft objective/preference; matched probes, survival/resource counterexamples | Held-out behavior report distinguishes competence, preference, uncertainty and fallback contribution |
| P8A.4 Export/native inference | Model/manifest, preprocessing/mask/action selection; candidate runtime adapter | Python/export/native agreement within tolerances, legal action consistency, complete native encounters |
| P8A.5 Async integration | Immutable snapshots, bounded resources, deadlines, version checks and conventional fallback | Late, malformed, missing, incompatible, all-invalid and shutdown/overload paths controlled |
| P8A.6 Accessible workflow | Enemy behavior/observation selection within supported contract, preset recipe, training/evaluation status, approve/package model | Developer follows sample without editing core engine; rejected model is visibly rejected; ordinary game still runs |

<!-- P8A-status -->
| Task | Status | Evidence |
|---|---|---|
| P8A.1 | Implemented: observation v1, catalogue, masks, fingerprint, 1v1 environment on the real TurnEncounter | `tests/policy/PolicyTests.cpp`; `game/policy/include/Policy.md` |
| P8A.2 | Implemented: C bridge + Python (numpy, PyTorch, CPU); PPO recipe; seeds, time and resources recorded in each model | `tools/policy/README.md`; models' `training` block |
| P8A.3 | Implemented: soft preference reward, control model, matched probes, counterexamples, thresholds declared before final runs | `testing/p8a-learned-enemy.md` §2–3; `docs/testing/p8a/*.json` |
| P8A.4 | Implemented: `umbra.policy` JSON, built-in C++ MLP; parity 6.6e-6, 200 native fights | `PolicyTests` (parity, native fights) |
| P8A.5 | Implemented: async decider (tagged answers, deadline = enemy pause, one job, join on shutdown), conventional fallback | `PolicyTests` (decider); `apps/umbra/GameFlow.md` |
| P8A.6 | Implemented: preset recipe, evaluate/approve (refusal visible), `UmbraTool policy-check`, editor field, game refusal message | `tools/policy/README.md`; Windows run open |
<!-- /P8A-status -->

**Exit gate:** one accepted turn-based policy executes offline and demonstrates the measured learned preference under declared tests, not a hidden forced branch. Full result must include failures, limitations and explicit acceptance thresholds. Verify T19, T23–T31, T41, T47, T52, T54–T55 and T59–T60.

Do not replace a failed experiment with unreported heuristic behavior and call the learned feature complete. The game may still ship a development build with conventional fallback, but P8A remains open until its evidence passes.

### P8B — AI-assisted quest and dialogue authoring

**Dependency:** stable content schemas, validators, transaction/undo capability. No runtime learned policy dependency.

| Task | Implement | Required evidence |
|---|---|---|
| P8B.1 Provider spike | One local model/tool bridge, constrained request/result contract; cancel/progress and size limits | Valid structured draft; controlled malformed output and local provider failure; no hidden cloud fallback |
| P8B.2 Patch proposal | Scope-limited quest/dialogue changes with base revision; validate references/semantics | Missing references and conflicting newer edits rejected; source assets outside scope unchanged |
| P8B.3 Approval/undo/playtest | Visible diff, explicit approval, reversible atomic commit, scenario execution | Accept, undo and retry content change; failed validation never partially applies |
| P8B.4 Novice workflow | Guided example, actionable diagnostics, manual editing remains first-class | Creator generates and edits a playable draft; shipped game excludes authoring dependency |

**Exit gate:** a usable, testable, reversible authoring workflow with one selected local integration. A provider that only returns prose without a validated patch is not sufficient. Verify T20–T21, T42, T48, T51, T58 and the relevant content tests.

<!-- P8B-status -->
| Task | Status | Evidence |
|---|---|---|
| P8B.1 | Implemented: Ollama `/api/chat` bridge (`engine/localai`), local addresses only, streamed with JSON-schema `format`, 64 KiB / 600 s limits, instant cancel, background `ChatJob`; real model not run | `tests/localai/OllamaTests.cpp` (stand-in server); `LocalAi.md` |
| P8B.2 | Implemented: draft → additive changes (one quest, NPC conversation lines + offer), scope check, game loader, base revision | `tests/authoring/QuestDraftTests.cpp`; `QuestDraft.md` |
| P8B.3 | Implemented: preview, play-through (real `GameSession`, fights assumed won), approve = one undo step, conflict → check again, fix request; `UmbraTool draft --apply` with `.before-draft` | `QuestDraftTests`; `Umbra.Tool.Draft*`; editor not opened |
| P8B.4 | Implemented: creator guide §2b, troubleshooting, actionable `ai.*`/`draft.*` diagnostics, RPG forms open on the new quest | `docs/creator-guide.md`; Windows checklist open |
<!-- /P8B-status -->

A cloud adapter can be a later optional task; credentials, data disclosure and spend controls require explicit design. It is not necessary to complete offline/local-first P8B.

### P8C — AI texture generation

**Dependency:** P7 authoring documents, P8B `Umbra::LocalAi`. **Goal:** creators make tileable
surface textures with an image model on their own PC and apply them like any other edit. Full
plan, options and open choices: [`adr/0002-ai-texture-generation.md`](adr/0002-ai-texture-generation.md).

| Task | Implement | Required evidence |
|---|---|---|
| P8C.0 Spike | Chosen generator on the user's PC; time, VRAM, tiling quality; recheck Ollama image generation on Windows | Recorded numbers and samples; default model chosen |
| P8C.1 Engine textures | Standalone PNG import with mipmaps, `umbra.material`, scene `material` field (validated, reloadable), world-scale projection, editor picker | Asset/scene tests; tiled surfaces without stretching (Windows) |
| P8C.2 Image provider | `ImageProvider` + ComfyUI adapter, local only, cancel/limits, installed-model list | Stand-in server tests; named failures |
| P8C.3 Texture checks | Seamless step, seam score, size rules, provenance and license table | T61, T63, T65 |
| P8C.4 Editor and tool | AI texture window (variations, tiled preview, apply/undo, style), `UmbraTool texture` | T62; Windows checklist |
| P8C.5 Records | Creator guide, testing record | Docs |

**Exit gate:** a creator generates, previews, applies, undoes and plays a tileable texture made
locally; the game build contains no image-generation code (T66); no non-local address (T64).

<!-- P8C-status -->
| Task | Status | Evidence |
|---|---|---|
| P8C.0 | Done by the user on Windows (2026-11-08): ComfyUI + SDXL texture made and applied | `testing/p8c-ai-textures.md` §3c |
| P8C.1 | Implemented: mip chains (all textures), world-space UVs, material files, scene `material`, hot reload, editor picker, sample bricks; GPU not run | `TextureToolsTests`, scene material tests |
| P8C.2 | Implemented: ComfyUI bridge, checkpoints list, port discovery, `ImageJob` | `ComfyUiTests` (stand-in server) |
| P8C.3 | Implemented: seamless step, seam score, size rule, provenance, license table, validate + package warnings | T61, T63, T65 |
| P8C.4 | Implemented: editor AI texture window (in-scene preview, apply/undo), `UmbraTool texture`, style file; editor not opened | Windows checklist open |
| P8C.5 | Implemented: guide, docs, record | `testing/p8c-ai-textures.md` |
<!-- /P8C-status -->

### P8D — AI 3D models through ComfyUI

**Dependency:** P8C ComfyUI bridge. **Goal:** props made locally from words or a picture, checked by
the game's importer, placed with undo. Plan and decisions: [`adr/0003-ai-model-generation.md`](adr/0003-ai-model-generation.md).

<!-- P8D-status -->
| Task | Status | Evidence |
|---|---|---|
| P8D.0 | Done by the user (2026-11-08): exported and run on an 8 GB GPU; fixes `#UMBRA-62` | `testing/p8d-ai-models.md` §3c |
| P8D.1 | Implemented: workflow load/check, run values, upload/run/download, shared `RunGraph`, `BatchJob` | T67–T69 |
| P8D.2 | Implemented: concept style, save with provenance, importer check, placement, TRELLIS.2 license entry | T70 |
| P8D.3 | Implemented: editor AI model window, `UmbraTool model`; editor not opened | Windows checklist open |
| P8D.4 | Implemented: ADR 0003, ModelGen.md, workflow README, guide, record | docs |
<!-- /P8D-status -->

### P9 — PC release gate

**Dependencies:** complete reference game and creator workflow; accepted P8A and P8B features. **Goal:** a reproducible, distributable PC engine/reference-game package, not just developer-machine success.

Define reference hardware/resolution/settings/scene and numerical frame/resource/iteration thresholds from measured evidence. Run Debug/Release and relevant compiler/sanitizer checks actually supported by the repository. Perform long-run traversal/combat/reload/save cycles, clean-install testing, missing/corrupt asset/model handling and offline startup. Validate license/provenance notices for dependencies, source assets and models; do not assume unrestricted library choice grants redistribution rights.

Verify package composition: shipping game has no editor, training environment, authoring model, build-tree path, secret credential or required service. Ship the accepted local policy and required content in the declared offline package. Validate runtime loader/distribution needs on a clean machine. Record deliberate optional downloads separately.

**Exit evidence:** creator walkthrough plus full player journey, independent-mode fixtures, accepted policy evaluation, saved-state recovery, graphical/physics checks, packaging/license inventory and measured performance report. All applicable P9-blocking tests in section 10 have evidence; deferred-platform/service cases are explicitly not P9 blockers.

<!-- P9-status -->
| Task | Status | Evidence |
|---|---|---|
| P9.1 Performance measurement | **Measured on the reference PC** (user, 2026-11-08; RTX 3050 Laptop, 1920×1080): 3.47 ms average, p99 ≤ 6.2 ms in every area, 250 MB peak, no growth | `testing/p9-release-gate.md` §3b |
| P9.2 Long-run state | Implemented and passing: 300 fights, save round trips, damaged-save rescue | T74 |
| P9.3 Package composition/licenses | Implemented: `UmbraTool package-check` in `package.bat`, run-time asset lookup (no build paths used), notices + `licenses/` | T75; `Package.md` |
| P9.4 Gate runs | **Open:** benchmark on the reference PC (decides O18), package + clean install, journey on the package, P3/P4/P6/P7/P8A checklists | `testing/p9-release-gate.md` §4 |
<!-- /P9-status -->

### P10 — Playable web export

**Dependencies:** P9 and W0 findings. **Goal:** export the supported game, not the editor, with no required gameplay backend.

| Task | Implement | Required evidence |
|---|---|---|
| P10.1 Browser graphics/platform | Selected graphics/shader path, asset/profile adaptations, async browser lifecycle and input | Reference scene and both modes work in declared browsers; unsupported capability reports clearly |
| P10.2 Browser policy | Validated model operator/format path; worker/bridge as needed; consistent masks/selection | Native/browser output and gameplay checks with tolerances; render remains responsive |
| P10.3 Save persistence | Browser-backed storage adapter, quota/error handling, explicit save export/import | Restart/reload preserves data under supported conditions; denied/full/cleared storage handled visibly |
| P10.4 Offline installation | Version-coherent app/assets/model caching and readiness indicator; controlled update behavior | Complete offline-ready install works with network unavailable; partial cache is not reported ready |
| P10.5 Lifecycle/performance/package | Tab hide/suspend, graphics loss, audio/input restrictions, memory/download limits, profile support | Repeated suspension/resume and failure matrix; packaging does not depend on Windows paths/editor |

<!-- P10-status -->
| Task | Status | Evidence |
|---|---|---|
| P10.1 | Implemented; **renders on the user's GPU** (built-in browser, 2026-11-08) | `testing/p10-web-export.md` §3b |
| P10.2 | Implemented: C++ network in WebAssembly, inline decider; parity with NumPy passes in wasm | Node suites 13/13 |
| P10.3 | Implemented: IndexedDB saves, visible failures, export/import, Continue | browser check 9/9 |
| P10.4 | Implemented: versioned all-or-nothing service worker cache, readiness, update on reload | browser check (offline start) |
| P10.5 | Implemented: no-WebGPU and device-lost messages, hidden-tab pause, local server script | browser check; user checklist |
<!-- /P10-status -->

ONNX Runtime Web provides a browser adapter candidate, but operators/backends and actual model compatibility require testing [S6]. This may be a JavaScript/worker bridge to the C++ WebAssembly game; do not assume linking the desktop inference library unchanged is the solution.

Service workers can serve cached resources offline [S9]. Browser storage has quota and eviction behavior [S10]. Therefore keep explicit offline readiness, coherent package updates and save export/import; never promise that browser storage cannot be cleared or evicted. First visit is not equivalent to a complete offline installation.

If worker/thread deployment needs special headers or capabilities, test the actual intended hosting configuration. A simple single-thread simulation remains an option. No multiplayer server is required merely to host downloadable browser assets.

**Exit:** supported full game loop and learned turn policy work in declared browsers after offline installation; failures have visible recovery. Verify T35–T38, T43–T45, T52 and T56 with platform-specific evidence.

### S1 — Project structure and components (user request, 2026-11-08)

| Item | Status | Evidence |
|---|---|---|
| S1.1 Layers: `engine/`, `game/`, `projects/<name>/` with one CMake build; run-time asset lookup by project name | Implemented (`#UMBRA-72`) | all suites; package check refuses a project assets path (T75) |
| S1.2 Components: built-in four under `components`, project schemas + `UMBRA_COMPONENT` C++, older scenes migrate | Implemented (`#UMBRA-71`) | T77 |
| S1.3 Editor: Add component / remove, schema fields, Code window opening VS 2022, New component template | Implemented; Windows check open | ADR 0004 |
| S1.4 Web build picks up editor changes (`UmbraTool web-pack`, run by `serve-web.bat`) | Implemented (`#UMBRA-70`) | T76; check-web |

### S2 — Source plugins (user request, 2026-11-08; ADR 0005)

| Item | Status | Evidence |
|---|---|---|
| S2.1 Plugin manifests, project.json, CMake loader (requirements, `requiresAny`, `UMBRA_PLUGIN_*`) | Implemented (`#UMBRA-74`) | configure messages; T78 |
| S2.2 Game systems and tools moved into 9 plugins (modules, tests, tools, assets) | Implemented | all suites (reference set) |
| S2.3 Hosts without plugin names: `UMBRA_EDITOR_PANEL` + `EditorHost`, `UMBRA_TOOL_COMMAND` | Implemented; editor Windows check open | builds without ai-authoring show no AI windows/commands |
| S2.4 Optional combat modes and learned AI really optional | Implemented | T78: turn-only (+learned AI, no AI tools) 35/35, real-time only 31/31 |
| S2.5 Layered assets (`assets::AssetFolders`: project over plugins) in game, editor, tools, web pack, package | Implemented | gameplay sounds from `third-person`; web asset pack includes them; check-web 11/11 |

### S3 — Gameplay UI framework (user request, 2026-11-08)

User decisions (2026-11-08): screens are **data** (`assets/ui/*.ui.json`, editable with a live
preview in the editor) with C++ only for new widget kinds and game bindings; the game **draws its
UI itself** (same look on PC and web; Dear ImGui stays for debug and the editor); the first slice
**replaces all current game UI and adds inventory/equipment, skill tree and quest log screens**.

| Task | Work | Exit evidence |
|---|---|---|
| S3.1 UI drawing | `core::DrawList` gets UI quads (rect, image, 9-slice, text) with clipping; a font atlas from a TTF (OFL font, stb_truetype); the Vulkan and WebGPU renderers draw them after the scene | Same screen pixels-close on both renderers (capture); text crisp at 1x/2x; no ImGui in the game's normal UI |
| S3.2 Screens and widgets | `engine/ui`: widget tree from JSON (panel, text, image, bar, button, list, grid, tabs), anchors + stacks layout, a skin file (colors, fonts, panel images), bindings `{hero.health}` from a C++ data provider, actions sent to C++ handlers; validated with diagnostics like scenes | Headless layout/binding tests; bad screen files named with field paths |
| S3.3 Input and focus | Mouse, keyboard and gamepad navigation (focus moves, confirm/back), modal stack, pause interaction | Headless navigation tests; manual gamepad check |
| S3.4 Replace the current UI | HUD (HP/EN/XP, prompts, messages, warnings), start/pause/death menus, conversation and fight menus as screens; the browser page's HTML HUD replaced by engine drawing | Journey on PC and in Chrome/Edge with the new screens; check-web updated |
| S3.5 New RPG screens | Inventory and equipment, skill tree (unlock), quest log, in the `rpg`/`story` plugins' assets with their bindings | Headless session tests for equip/unlock through the screens' actions; manual play |
| S3.6 Editor | UI window: open a screen, live preview with sample data, edit properties (undoable, validated) | Editor checklist on Windows |

| Item | Status | Evidence |
|---|---|---|
| S3.1 UI drawing (Vulkan through the ImGui background list, WebGPU pass, font atlas) | Implemented (`#UMBRA-75`); GPU check on Windows/Chrome open | no WebGPU validation errors; CPU-rasterized previews |
| S3.2 Screens, skin, bindings, diagnostics | Implemented | T79 |
| S3.3 Keyboard, mouse, gamepad focus; modal stack | Implemented; gamepad check open | T79 |
| S3.4 HUD and all menus as screens; HTML HUD and `PauseMenu` removed | Implemented | check-web 13/13 |
| S3.5 Inventory/equipment, skills, quest log (`rpg-ui` plugin) | Implemented (in the new `rpg-ui` plugin rather than `rpg`/`story`, so a game can drop or replace its look) | T79: the screens send equip/unequip/unlock; the rules' own tests in `rpg` |
| S3.6 Editor Game UI window; `UmbraTool ui-preview` | Implemented; Windows editor check open | `Umbra.Tool.UiPreview`; Editor.md |

### S4 — AI sound, music, turn menu and hotkeys (user request, 2026-11-08; ADR 0007)

User decisions (2026-11-08): ACE-Step 1.5 for music and Stable Audio 3 for effects, both through
the local ComfyUI; Ogg music and WAV effects; a music system and RPG sound hooks; no extra AI tool
for UI design (mockups from SDXL or other tools), but measure how well screens and code come from
a prompt, using the turn menu (Attack / Item / Flee, target arrow) and real-time hotkeys.

| Item | Status | Evidence |
|---|---|---|
| S4.1 Turn menu: Attack → move → target arrow over the fighter; Item; Flee; Esc steps back | Implemented (`#UMBRA-77`) | T79 additions; ui-preview pictures |
| S4.2 Real-time hotkeys 1-4 (skills or items), hotkeys screen, saved | Implemented | `Umbra.Game` hotkey save test; T79 |
| S4.3 Ogg music (stb_vorbis), streaming with crossfades, Music component, browser build | Implemented; listening on Windows open | T80; WebAssembly 15/15 |
| S4.4 Skill/item `"sound"`, default hit sound | Implemented | `Umbra.Rpg` |
| S4.5 AI sound: ComfyUI graphs, loop/trim/normalize, Vorbis/WAV, provenance, editor window, `UmbraTool sound` | Implemented; real-model run open | T81; `Umbra.Tool.SoundLocalOnly` |

### S5 — Agent prompt: editor Agent window and MCP server (user request, 2026-11-08; ADR 0008)

**Status (2026-11-08): implemented** (deferred, then started by the user after P11; `#UMBRA-80`). Evidence `testing/s5-agent-prompt.md`; how it works `plugins/ai-authoring/modules/agent/include/Agent.md`.

| Task | Status | Evidence |
|---|---|---|
| S5.1 Tool registry (7 read, 10 change, 3 generate tools; `Edits` working copies, one undo step) | Implemented | T83 |
| T84 | Environment at noon, dusk, midnight, indoors, a wrapped and a stopped clock; more than 16 lights; flicker; the shadow box's texel snapping; `light`/`environment` in a scene file (defaults, bad values, a second environment, save round trip); engine schemas and a project schema with a reserved name; a save with an hour, an old save without one, an out-of-range hour | Sun up at noon and the moon at night, colors from the right keys; indoor has no sun or shadows; the clock wraps at 24 and stands still at day length 0; the 16 nearest lights kept; flicker within 0.5..1; moving the camera less than a texel doesn't move the box; values checked against the schemas, the second environment warned about and ignored, the save writes the current hour; the reserved name warned about; the hour restored, missing or bad hours ignored | S6 |
| T85 | Tonemap at black, middle gray, very bright and negative light; neutral, warm, gray and high-contrast grades; a LUT image (inverted), one of the wrong size; exposure, bloom and vignette on whole pixels; quality levels; the `post` component with a LUT, defaults, saving, a wrong-sized or missing LUT, an out-of-range value, a second post | Middle gray kept near 0.2, never past white; a neutral grade changes nothing (within one 8-bit step), warm is redder, gray has equal channels, contrast darkens darks and lightens lights; the LUT image applied, a bad one refused and ignored; exposure and bloom brighten, the vignette leaves the middle and darkens corners; Reduced starts smaller with fewer levels; the component read, extracted with its LUT, saved, bad ones refused, the second warned about and ignored | S7 |
| T87 | A skeleton with known joints and clips; the packed hero.glb; a graph with idle/walk/attack/dead | Clips sampled at known times give the expected poses (lerp, slerp, step, loop, hold); the palette is identity at rest; the importer keeps 24 joints and six named clips, normalised weights and a rest-pose box of -1..1 in height, all through the `.umodel` cache; the animator picks the first matching state, fades, plays a one-shot to its end, follows the speed rate | S8 |
| T88 | Stand-in ComfyUI with rig and motion workflows using the UniRig / HY-Motion node names | Wrong workflows refused (`workflow.model`, `workflow.text`, UI format); the model is uploaded into the loader's folder and named there; text, length, seed and character filled in; a GLB result preferred over an FBX, a path on disk understood; results saved with `.source.json`; a result without a skeleton not kept; without Blender an FBX gives `ai.convert` naming UMBRA_BLENDER | S8 |
| T89 | A graph with events at 0, 0.4 and 0.6 s and a looping walk; an effect with a burst and a rate; the six shipped effects; a scene with a `vfx` component; RPG content naming effects | Each event fires once per crossing at 60 and 5 frames a second and in one long frame, and again each lap (at most four laps per frame); the same seed gives byte-identical particles, never above the effect's limit; one-shots end, attached emitters stay and are not doubled; bad effect files are named by field; the component survives save/load; a missing effect named by a scene, skill, item or status fails validation | S9 |
| T90 | The reference town (seven residents); hand-written town files with one mistake each; seeds 1-10 | The town loads against the RPG content and mistakes are named by field; one seed gives one week (golden fingerprint, every compiler) and another seed another; a fork run ahead equals the original run ahead and leaves it untouched; illegal intents change nothing; over thirty days needs, places and coins always add up; a saved town continues exactly and a save from a changed town file is refused; an illegal proposal equals no proposal; a year runs in well under a second | S21 |
| T91 | The reference town over 30 days; the same town with no social activities; a favour asked and skipped; a favour asked and kept; one fact of every kind | Facts are known to their witnesses and then travel only by talk (none travel when nobody meets); a skipped favour is known to the asker at once and to others days later, who then think less of the player and say so in a fixed sentence; a kept one is thanked and warms the asker when they hear; every kind has its sentences; the chronicle lists what the player was told, once each; memories survive a save | S22 |
| T92 | A scripted stand-in model answering good lines, refused lines, nothing, nonsense numbers, and slowly | Good lines are shown and kept; each guard check refuses what it should with its reason; refused, empty, late and unprepared lines give the template and leave the town's state as without a model; picks outside the legal list are never used; a slow model never delays a talk; answers from before a load are dropped; saved words are shown again without a new request | S23 |
| T93 | Each kind of trouble made to happen; ten wrong plans; a plan with forbidden, early and over-budget trouble; a beat nothing leads to and one that has become impossible; the reference plan on 1,000 seeds, and on 300 with and without rehearsal | Trouble does what it says and coins add up; the gone are out of every rule; each plan mistake is named; the filter refuses what the plan forbids; the beat is forced on its last day with a warning, the impossible one is missed; same seed same story through a save; every playthrough finishes the plot, nothing against the plan, each beat reached at least three ways, fewer forced beats with rehearsal | S24 |
| T86 | A material with a surface shader; one with a missing shader file and with a wrong name; names with spaces; a broken surface shader compiled by the real DXC | The shader path and name kept and written back; missing file and bad name refused (`material.shader`); the broken shader refused, its error naming the surface file and line 5 | S7 |
| S5.2 Agent loop (schema-forced JSON plan, look rounds, dry run, fixes; runner stops at the first failed step) | Implemented | T83 (stand-in Ollama) |
| S5.3 Editor Agent window | Implemented; Windows check with a real model open | `testing/s5-agent-prompt.md` §4 |
| S5.4 `UmbraTool mcp` (stdio JSON-RPC, tools + build/run_tests, saved at once, editor reload) | Implemented | T83; a real stdio session in the cloud |

User decisions (2026-11-08): both an in-editor Agent window and an MCP server over one
tool registry; the in-editor agent uses local models only (Ollama tool calling); it shows a plan
for approval and lands as one undoable edit; it changes data only, while C++ features come from
external coding agents through MCP.

| Task | Work | Exit evidence |
|---|---|---|
| S5.1 Tool registry | Read and change tools over the existing documents and AI generators, JSON schemas | Headless tests: each tool's normal path, bad arguments named, changes undoable |
| S5.2 Agent loop | Local Ollama tool calling; plan (dry-run) -> approve -> run; stop on the first failed step | Stand-in Ollama tests: a scripted plan runs, a malformed call is refused, a failing step stops the rest |
| S5.3 Editor Agent window | Prompt, plan view, approve/cancel, per-step results, one undo | Windows editor checklist with a real model on a small task set |
| S5.4 `UmbraTool mcp` | MCP stdio server with the same tools plus build/test, local only | An MCP client lists and calls tools; a scene edit made through MCP validates and appears in the editor |

### S6-S17 — RPG flair, editor and agent (user request, 2026-11-08; ADR 0010)

**Planned.** The user chose every proposed improvement, added cutscenes, an animation/SFX event
system, rigging, custom shaders and post-processing, and asked for an example of each; RPG flair
first. Details, examples and the decisions to confirm per phase: `adr/0010-flair-editor-agent-plan.md`.
Each phase gets its own test IDs, evidence record and Windows checklist when it starts.

| Phase | Exit evidence (besides its example in the reference project) |
|---|---|
| S6 Lighting & atmosphere (**implemented**, T84) | Light and fog math checked headlessly; shadow map and lights on Vulkan and WebGPU; frame time within O18 on the user's PC |
| S7 Post & custom shaders (**implemented**, T85, T86) | Tonemap/bloom/LUT checked on CPU-rendered references; a custom shader compiles for Vulkan and WGSL; a bad shader is refused with its line |
| S8 Skeletal & rigging (**implemented**, T87, T88) | Skinned glTF import and clip sampling tested against known poses; crossfade; AI rig + motion workflow through a stand-in ComfyUI |
| S9 Events, SFX, VFX (**implemented**, T89) | Events fire once per frame crossing (also at low frame rates); particle simulation deterministic for a seed; effects named in RPG content are validated |
| S10 RPG systems | Shop, loot, locks and dialogue conditions/consequences through the real `GameSession`; saves keep gold, flags and opened chests |
| S18 Party & characters | Party turn fights and real-time switching through the real `GameSession`; recruit/dismiss by dialogue consequence; per-member equipment and skills; saves keep the party (old saves load as a party of one) |
| S19 Resource previewers | Sound player state (play/stop/loop/seek) and waveform tested headlessly; image/model/material previews rendered off-screen and compared with references; every asset type the inspector links has a preview or opens Visual Studio |
| S20 Editor settings | Settings file round-trips; a broken file falls back to defaults with a message; a hotkey conflict is refused; the overlay toggle reaches the game |
| S21 World tick & residents (**implemented**, T90; MSVC run open) | Same seed, same world after N days (GCC, Clang, MSVC); a fork run forward equals the original run forward, and never changes the original; offscreen and onscreen residents agree at hand-over; saves round-trip the world; fast-forward speed recorded |
| S22 Memory, chronicle & rumours (**implemented**, T91) | Events reach only residents who saw or heard them; rumours spread by rule; chronicle order stable; template lines for every event kind |
| S23 Built-in model | Stand-in model: invalid, late and malformed proposals dropped with the conventional choice used; no frame waits on the model; stored output replays from a save; shutdown with work outstanding; package-check finds the model and its notice |
| S24 Storyteller & outcome plan | Plan validation; forbidden outcomes never occur and required beats always occur across 1,000 seeds; pacing budget kept; report lists outcome spread and repetition |
| S25 The town | Scripted full playthroughs on several seeds finish the main plot with different chronicles |
| S11 Cutscenes | Timeline sampling; trigger once; skip leaves the world in the end state; validated files |
| S12 Prefabs | Prefab instances with overrides load like the game; a prefab change reaches every instance; undo |
| S13 Play in editor | Stop restores the exact document state; play uses the same `GameFlow` as the game |
| S14 Graph editors | Graph edits round-trip to the same JSON; invalid links refused |
| S15 Agent chat & vision | Follow-up and "here/this" context in the request; `look_at_scene` image sent to a stand-in vision model; new tools tested like S5 |
| S16 Recipes & scatter | Scatter places N non-overlapping, ground-snapped entities for a seed; recipes validated |
| S17 Code agents | Scaffolded component builds and passes its test; MCP resources listed and read; agent-eval scores a scripted model |

### P11 — Mobile

**Status (2026-11-08): implemented, emulator run open.** Target chosen by the user: Android, the emulator first (x86_64), then arm64 phones; touch and gamepad; landscape only (ADR 0009).

| Task | Status | Evidence |
|---|---|---|
| P11.1 Android build: `ANDROID` CMake branch (game only, `libmain.so`), Gradle project with SDL's Java activity, game-only vcpkg manifest, `scripts\build-android.bat` | Implemented; Windows build open | configure check on Linux; `testing/p11-android.md` §4 |
| P11.2 `game.pack`: assets + Vulkan 1.1 shaders in the APK, unpacked once per version | Implemented | T82 (`Umbra.Authoring` game-pack test) |
| P11.3 Lifecycle: save, pause, release the surface in the background; restore on return; surface lost handled | Implemented; emulator check open | `testing/p11-android.md` §4 |
| P11.4 Touch: floating stick, camera drag, buttons, fight hotkeys; gamepad sticks with dead zone | Implemented | T82 (`Umbra.App` touch tests); picture check |
| P11.5 Phones: arm64 build, thermal and sustained frame time | Open (after the emulator) | — |


Select the initial OS/device tier before implementation. Earlier Android ARM64 intent is historical context, not a newly confirmed permanent API-level requirement. Reuse platform-neutral rules/assets and choose appropriate graphics/inference backends, lifecycle/storage and packaging adapters.

Add touch/controller layout as needed, pause/resume/audio focus, orientation policy, save recovery, model/resource limits, packaging and installed-offline checks. Measure thermal behavior and sustained frame time on real devices; desktop figures do not establish mobile readiness.

**Exit:** declared devices pass the supported reference journey, independent-mode/runtime policy checks, suspend/resume/termination/save recovery and resource budgets. Other mobile OSs and stores are additional scoped ports, not automatically covered.

### F1 — Later, independently approved work

Community records/retraining/model delivery, learned real-time enemy behavior, mod/plugin capability, additional platforms, 2D/2.5D, advanced rendering and other research require separate approval and acceptance criteria. Their presence here creates no initial code/dependency obligation.

## 9. Fast iteration and agent tooling

### 9.1 One tool surface

Provide thin, documented capabilities for validating content, building a selected target, running a selected headless scenario, inspecting a schema/state snapshot, executing regression tests and packaging a game. A JSON output mode should carry stable error code, entity/definition ID, file/field, severity, explanation and recovery suggestion when useful.

The editor and AI tools call the same command/validation implementation. Do not build an elaborate network tool server, agent orchestration framework or MCP integration until a simple CLI/API demonstrably fails a required workflow. Agent friendliness begins with explicit contracts and small runnable fixtures.

### 9.2 Build scope

Keep heavy graphics/model/import libraries private to their modules where practical. Core rule edits should not rebuild unrelated graphics implementation. Avoid unnecessary public-header coupling, broad includes, generated-code churn and over-splitting compilation targets. Measure before adopting PCH, unity builds, caching, modules or new build tools; optimize the actual bottleneck, not a hypothetical one.

Preserve the project's pinned dependency baseline. When adding a dependency, record purpose, supported targets, license/distribution constraints, transitive weight, pin and removal/revisit criterion. A candidate in this document is not a command to install it.

### 9.3 Edit contract

| Edit type | Normal path | Failure/recovery |
|---|---|---|
| Supported data | Validate/stage/commit at safe point | Keep old valid graph; structured error |
| Shader/asset | Build/check artifact, publish compatible replacement, retire old resources safely | Keep last valid artifact; unsupported layout requires explicit rebuild/restart |
| Gameplay C++ | Small incremental build; restart play; restore compatible checkpoint | Keep known-good executable/content pairing; report compile/link failure |
| Structural/schema | Version/migration decision, rebuild, migrate or clean restart | Preserve original data; refuse incompatible load clearly |
| Learned model | Offline evaluation and compatibility check, load before encounter | Reject/retain previous accepted model or conventional fallback |

Native hot reload remains optional. Data hot reload and a dependable restart are the required foundation. Undo/redo is an authoring transaction feature, not a general rollback of arbitrary worker side effects.

### 9.4 Agent execution contract

A task has a milestone ID, short intent, prerequisite evidence, explicit exclusions, changed interfaces, implementation plan, tests and completion evidence. Work in small runnable increments. Keep code short and direct. Use simple step-by-step comments only where the code is not obvious. Put deeper explanations (math, rendering, physics, memory management) in a markdown file beside the code, never in long comment blocks.

Do not claim a code edit was built, a screenshot captured, a model trained, or a GPU run validated without evidence. Repository tests and manual checks remain separate. Update progress only after running the relevant verification or marking it honestly not run.

Use direct source inspection to learn current names; do not invent class names or CMake presets from this conceptual plan. Keep speculative architecture out of public headers until a concrete task needs it.

## 10. Acceptance and recovery matrix

This is an initial **60-case register**, not an assertion that every possible future corner case is enumerated. T01–T38 retain the previous baseline's identities; T39–T60 extend it for confirmed decisions and operational gaps. All cases begin **not run** in this document. Add a focused regression whenever implementation exposes another failure mode.

“Gate” names the first applicable stage; rerun related tests after changes to their contracts. Later-only web/service cases do not block the PC milestone. The following behavior is a proposed acceptance policy until implemented and verified.

| ID | Case | Required behavior / observable assertion | Gate |
|---|---|---|---|
| T01 | Stale entity/resource handle | Reject stale generation; no use-after-free or accidental alias after slot reuse | P3 |
| T02 | Corrupt/partial asset reload | Keep last good version; no mixed old/new dependency graph | P3 |
| T03 | Missing/circular content reference | Identify offending ID/reference chain; validation/package fails clearly | P3/P7 |
| T04 | Hierarchy cycle or deleted parent | Reject cycle; apply explicit parent-deletion policy; no traversal loop | P3 |
| T05 | Focus loss/suspend during movement | Clear/restore input ownership correctly; no stuck movement or excessive resumed delta | P4 |
| T06 | Repeated animation/contact event | Only intended hit count applies per activation/target; no duplicate cost/effect | P5 |
| T07 | Target dies after action selection | Revalidate; cancel/retarget only under recorded rule; no dead-target unintended effect | P5 |
| T08 | Interrupted action after cost commitment | Defined refund/cooldown behavior; cost neither lost nor refunded twice | P5 |
| T09 | Status spans combat-mode transition | Explicit domain/pause/expiry/mode mapping; no hidden turns-to-seconds conversion | P5 |
| T10 | Multiple deaths/rewards in one update | Stable declared resolution order; rewards/events idempotent | P5/P6 |
| T11 | Equipment changes resource maximum | Explicit clamp/recompute policy; no invalid HP, repeated modifier or unintended heal | P5 |
| T12 | Cyclic tree or repeated unlock | Invalid prerequisites rejected; points spent once and only on successful transaction | P5 |
| T13 | Inventory full during quest reward | Defined pending/overflow/rejection behavior; quest/item/currency state remains coherent | P6 |
| T14 | Early or repeated quest events | Explicit retroactive/prospective policy; no repeated completion reward | P6 |
| T15 | NPC unavailable/conversation interrupted | Interaction can end; quest failure/recovery state is defined; no softlock | P6 |
| T16 | Save during unsafe action/inference | Queue or refuse visibly at safe boundary; snapshot committed state only | P6 |
| T17 | Termination during write/full/read-only disk | Preserve previous valid save; no false success; failure visible and retry safe | P6 |
| T18 | Older/newer/incompatible save schema | Explicit migration or refusal; preserve original bytes and active valid world | P6/P9 |
| T19 | Worker returns after load/restart | Reject stale world/session/encounter/actor/decision identity before mutation | P6/P8A |
| T20 | Schema-valid but unsolvable generated quest | Scenario/human gate detects supported failure; parsing alone never grants approval | P8B |
| T21 | Patch touches unintended content | Scope/diff/approval rejects; no partial external edit; undo returns approved baseline | P8B |
| T22 | Failed code build plus content edit | Known-good executable/content pairing remains runnable or explicitly selectable | P7 |
| T23 | Training reset leaves hidden state | Repeated seeded fixture exposes leaks; reset queues/cooldowns/RNG/subscriptions/terminal state | P8A |
| T24 | Timeout versus actual terminal outcome | Termination and truncation recorded distinctly; no fabricated post-terminal turn | P8A |
| T25 | All-invalid mask/non-finite output | Safe defined wait/fallback/error; no invalid indexing or probability normalization | P8A |
| T26 | Observation/action order or normalization changed | Manifest compatibility rejected before execution; correct tensor dimensions alone insufficient | P8A |
| T27 | Skill semantics changed with same tensor shape | Ruleset/content version check forces compatibility evaluation | P8A |
| T28 | Model hangs/returns late | Decision deadline causes one fallback; late result cannot overwrite committed choice | P8A |
| T29 | Exported/native model disagrees with trainer | Parity and native-encounter gate rejects; masking/preprocessing path included | P8A |
| T30 | Policy exploits reward/simulation loophole | Held-out/adversarial tests fail promotion; report mechanism rather than reward score only | P8A |
| T31 | Async inference still starves game resources | Measure whole-game impact; bound worker/resource usage and disable/degrade provider safely | P8A/P9 |
| T32 | Update requested mid-encounter | Stage only; activate at approved boundary; preserve rollback identity | F1 only |
| T33 | Cloud failure/withdrawn consent/upload retry | Offline game unaffected; no unauthorized upload/retry; queue and deletion policy bounded | F1 only |
| T34 | Malformed/poisoned community record | Reject/quarantine; never automatically promote contributed data to a distributed policy | F1 only |
| T35 | Browser suspend/GPU loss/storage eviction | Declared pause/recovery/reinstall/save-import UX; no false promise of preserved evicted data | P10 |
| T36 | Unsupported browser graphics/model operator | Detect capability; tested fallback or clear unsupported message, not silent broken play | P10 |
| T37 | Non-ASCII/localized text and paths | Stable IDs independent of display names; valid encoding, font/layout and diagnostics | P7/P9/P10 |
| T38 | Platform floating-point divergence | Tolerance-based relevant checks; recorded action/outcome replay; no false bitwise guarantee | P9/P10 |
| T39 | Turn-only game/fixture | Runs without constructing/linking an unnecessary real-time controller or advancing animation clocks for rules | P5 |
| T40 | Real-time-only game/fixture | Runs without turn state/scheduler; shared rules remain correct; no synthetic turns | P5 |
| T41 | “Learned” aggression implemented as forced branch | Inspection + paired held-out behavior tests reject hidden compulsory threshold override; report uncertainty and exceptions | P8A |
| T42 | Generated/imported text requests shell/secret access | Treat as data; no execution or new permission; patch and tool scopes preserved | P7/P8B |
| T43 | Clean offline install depends on cache/developer path | Fail packaging gate; required game content/policy present; headless/runtime have no hidden editor/service dependency | P3/P6/P9/P10 |
| T44 | Replaced resource still used by GPU/CPU reader | Deferred retirement waits for actual use completion; validation and sanitizer checks clean | P3/P4/P10 |
| T45 | Partial offline installation or inconsistent package update | Never signal ready with missing assets/model; preserve coherent prior version and report recovery | P6/P9/P10 |
| T46 | Illegal action partially mutates state | Rejection leaves costs, turn ownership, inventory, effects and rewards unchanged | P5 |
| T47 | Action legal in snapshot but illegal at commitment | Revalidate in shared rules; defined fallback/retry; no bypass by learned AI or player input | P5/P8A |
| T48 | Stale patch base or repeated undo/redo | Detect newer content; atomically apply/undo once; do not overwrite later manual edits silently | P7/P8B |
| T49 | Durable identity differs after reload/reset | Restore logical instances correctly without persisting transient handles; no duplicated item/quest ownership | P5/P6 |
| T50 | Oversized/corrupt/path-traversing asset payload | Bound reads/allocations, constrain supported paths, reject before uncontrolled processing or outside writes | P3 |
| T51 | Reload changes an active quest/skill/encounter | Pin compatible revision or migrate at defined boundary; no mixed definition semantics | P3/P7/P8B |
| T52 | Queue overload/provider failure during shutdown | Bound jobs and retries; reject late work; no dangling captures; documented cancellation/join behavior | P8A/P10 |
| T53 | Headless/game target leaks editor/training dependencies | Dependency/build fixture detects leak; no window, Vulkan, Python or model needed for core rules | P3/P7/P9 |
| T54 | Evaluation leakage/seed cherry-picking/hidden observations | Separate scenario sets; multiple runs where applicable; record seeds/uncertainty; reject disallowed observation access | P8A |
| T55 | Training rules drift from actual gameplay | Same native resolver or explicit equivalence tests; damage/cost/cooldown/death outcomes agree | P5/P8A |
| T56 | Pause/lifecycle breaks presentation or timing | No huge catch-up burst, duplicated input or orphan sound/action; coherent pause/recovery on supported platform | P4/P10/P11 |
| T57 | Asset rename/case change or duplicate durable ID | Identity policy prevents wrong-resource alias; rebuild references deterministically or report conflict | P3 |
| T58 | Local authoring failure silently uses cloud | No upload/credential use without explicit configuration/consent; diagnostics avoid secret leakage | P7/P8B |
| T59 | Untrusted or excessive-resource model package | Validate manifest/paths/operators/resources/provenance; safe test environment before acceptance; no automatic code execution | P8A/P9 |
| T60 | Trainer decision step/opponent timing mismatch | Reward/action opportunity/terminal handling matches native encounter; no extra player/enemy turn or double step | P5/P8A |
| T61 | Generated texture has a visible seam | Seam score flags it before apply; the creator sees the warning | P8C |
| T62 | Apply/undo/redo of a generated texture | Files written before the scene edit; undo removes only the reference; redo restores it; nothing half-applied | P8C |
| T63 | Material references a missing, oversized or non-power-of-two image | Loader/validator refuses with a named diagnostic; the last valid material stays | P8C |
| T64 | Image generator address is not this computer | Refused before connecting; no cloud fallback | P8C |
| T65 | Texture made with a non-commercial model is packaged | Packaging warns, naming the material and license | P8C/P9 |
| T66 | Game build links image-generation code | Dependency check fails | P8C |
| T67 | Image-to-3D workflow file missing, in UI format, or without one picture input / a 3D output | Refused with how to export it; nothing runs | P8D |
| T68 | Run values in the creator's workflow | Picture, seeds, face count and texture size set (also through Primitive nodes); the loaded workflow unchanged | P8D |
| T69 | Picture upload, run and GLB download; missing model; non-local address | GLB downloaded intact; `ai.model` / `ai.host` named | P8D |
| T71 | Exported workflow with switches, preview models and two save-able branches | Only the save node's dependencies run (switches resolved); the GLB comes from the save node, never a preview | P8D |
| T72 | Editor closed during a model run; ComfyUI restarted; a run longer than 30 minutes | ComfyUI keeps working and the next start fetches the model; a forgotten job is named (`ai.lost`); no total time limit while ComfyUI has the job | P8D |
| T74 | Long run: hundreds of fights in both modes with saves, loads and a damaged save | Every save round trip byte-identical; no fight state left over; backup rescues the damaged save; save size bounded | P9 |
| T75 | Package with a development program, an unlisted DLL, a key, an absolute path, a dependency on the source assets folder, missing content/policy | `package-check` names each (secret values never printed); AI leftovers and non-commercial models are warnings; `package.bat` makes no zip on errors | P9 |
| T77 | Entity with built-in and project components; older top-level parts; a project component with wrong or out-of-range fields; a type with no schema or no C++ | Both forms load the same; saving writes `components`; schema errors named; unknown type is a warning and only data; registered C++ behaviour starts and updates (Spin turns 90° in 1 s at 90°/s) | S1 |
| T78 | Projects with different plugin sets (reference; turn-only with learned AI and no AI tools; real-time only); an unknown plugin; a missing combat mode | Each set builds and passes its suites; the game, editor and tool show only their plugins' parts; clear configure errors | S2 |
| T79 | Screen and skin files (bad fields, fonts that are not TTF); focus, confirm, back and mouse on a modal screen; every reference screen with the sample data; the inventory and skill screens' actions; the CPU rasterizer | Problems named with field paths; actions come back to the game, disabled entries are never chosen; screens draw; previews match the GPU's blending and clipping rules | S3 |
| T80 | Ogg files at 48 kHz and other rates, junk; music fade-in, loop past the end, crossfade, fade-out, StopAll | Decoded or refused with the reason; no silence gap at the loop; old track gone after its fade; music unaffected by StopAll | S4 |
| T81 | Audio graphs for both models; model choice; a stand-in ComfyUI run; an effect with silence around it; 44.1 kHz music; saving twice | Right nodes and sampler settings; trimmed, mono, -1 dBFS; 48 kHz; loop point no bigger than a normal step; Ogg plays in the game decoder; `.source.json` with license; `_2` on the second save | S4 |
| T82 | Game pack: AI working files, a bad path, a range past the end, no shaders folder; touch: no finger yet, stick past its radius, finger lifted, camera drag, button held then lifted, menu opened, fight layout; gamepad axis in the dead zone and at full tilt | Only game files packed; bad entries refused and nothing marked installed (the next start unpacks again); controls hidden on a PC and over menus; stick clamped to 1; the button's key held then released, let go when a menu opens; fight shows 1-4 and Flee, not Use; every button inside the screen; axis 0 then 1 | P11 |
| T83 | Agent tools on a copy of the reference project; bad arguments; a change the loader refuses; a commit after a user edit; a plan with references and a generator; a scripted model that looks, answers badly, then well; a model that never fixes its plan; a non-local Ollama; a plan with a failing middle step; MCP initialize/list/call, a notification, an unknown method, bad JSON, a file changed on disk | Read tools list scenes, entities, assets, components, RPG ids; each bad argument named; nothing changes on refusal; changes stack and land as one undo step (scene + RPG content), all or none; generators and their users checked when run; the planner sends results and reasons back and returns a checked plan, gives up after its fix limit, refuses a non-local host; the runner stops at the failed step and commits nothing; MCP edits are saved at once and re-read changed files | S5 |
| T76 | Assets edited after a web build; damaged `assets.bin` | `UmbraTool web-pack` (run by `serve-web.bat`) packs them next to the page with a new offline version; the page shows the edit without a rebuild; AI working files left out; a short `assets.bin` is refused with a clear message | P10 |
| T73 | Placed generated model; collider added to a model entity | A static box fitted to the model's bounds at the entity scale (not the scale-derived default meant for the -1..1 cube) | P8D |
| T70 | Generated GLB the importer refuses | Not kept (all files removed); a good one is saved with provenance and never overwrites | P8D |

### Test layers

**Unit:** identity, transforms, modifiers, prerequisites, masks, serializers, state transitions.  
**Headless integration:** each mode alone, shared effects, quest/reward transactions, save/load/reset, training step.  
**Graphical/physics:** visible scene, movement/hits, validation, resize/minimize, resource lifetime, captures.  
**Model:** contract compatibility, parity, masks, behavioral evaluation, faults and resources.  
**Creator/end-to-end:** data-only content edits, generation approval/undo, package and reference game journey.  
**Platform:** clean offline install, browser cache/storage/lifecycle, selected mobile devices.

Use deterministic fixtures within a defined environment; use appropriate numeric tolerances outside it. Record committed AI actions and relevant outcomes for regression instead of assuming a random seed reproduces every inference/backend/thread ordering. Keep security/fault testing in controlled fixtures, not destructive trials against real player data.

## 11. Performance and release metrics

Hardware numbers were intentionally deferred. Add measurement now; assign final numerical gates after a representative workload and hardware profile exist. The target 60 FPS corresponds to roughly 16.67 ms per displayed frame, but CPU and GPU stages may overlap; do not simply add unrelated timing counters or treat an average as a frame-pacing guarantee.

| Measurement | Record | Acceptance timing |
|---|---|---|
| Frame pacing | Median/tail frame times, hitch counts, scene/settings, CPU/GPU bound state | Instrument P4; set numeric limits before P9 |
| Simulation | Per-step and subsystem times, fixed-step backlog/catch-up behavior | P4/P5 onwards |
| Memory | Native allocations/peaks, GPU resident/peak, model working set, scene transition/reload growth | P3 onwards; release stress at P9 |
| Inference | Cold load/warm latency/tails, worker count, queue depth, stale/timeout/fallback counts | P8A; model gate before P9 |
| Behavior | Legality, held-out outcomes, difficulty, style distribution, counterexample behavior, fallback share | P8A before model promotion |
| Iteration | No-op, gameplay source edit, common-header edit, shader edit, data reload, checkpoint restart | Establish P3/P7; optimize observed bottlenecks |
| Content pipeline | Cold/incremental import, dependency invalidation, peak memory, asset size | P3/P7 |
| Training | Wall time, samples/episodes, seed/config, resources, evaluation variance | P8A experiment; no invented budget promise |
| Package/platform | Installed/download/cache size, startup/load time, persistent-storage errors, offline readiness | P9/P10/P11 |

Benchmark the reference game both with conventional AI and with the actual packaged learned policy. Include combat, save/load, scene transitions and long repeated runs. Record all hardware/software/settings used. A fallback-only run does not prove learned-policy performance. A model microbenchmark does not prove whole-game performance.

Resource/failure limits can be conservative configurable defaults before final hardware selection, but label them provisional. Never mark the 60 FPS goal met without a named reference workload and measurements. Keep build speed and runtime speed as separate metrics.

## 12. Immediate implementation sequence

### B0 startup card

**Intent:** establish actual project status without changing scope.  
**Read:** root instructions, CONTEXT, dashboard, actual CMake files, dependency manifests, current status/testing records.  
**Do:** inspect source control state and target graph; compare source baseline with checkout; run supported baseline checks; identify already-completed later work.  
**Remaining (2026-11-01):** Windows `windows-msvc` configure/build/CTest, clang-cl and ASan presets, app launch with validation, CI run status, Phase 1 checklist sign-off. These need the user's Windows machine.  
**Do not:** rename modules, upgrade dependencies, add model runtimes, delete user changes or mark unavailable checks passed.  
**Deliver:** evidence note, observed milestone/task state, exact next task.

### P3.1 first implementation card, if still unmet

**Intent:** establish authoritative world/entity ownership and stale-reference protection before editor/gameplay/AI breadth.  
**Depends on:** B0; existing foundation.  
**Start from:** the existing uncommitted `engine/world` stub (`umbra::world::World`, `Umbra::World`). Add a `tests/world` Catch2 target following the other test folders.  
**Minimum scope:** one World owner, create/destroy APIs, generational transient handles, a clear durable-identity policy, enough component storage to support a transform later, error/diagnostic conventions.  
**Do not implement:** full ECS queries, scheduler/jobs, physics, rendering abstraction replacement, reflection generator or scripting.  
**Acceptance:** create/read/destroy; stale and foreign-world references rejected; slot reuse does not revive old handles; cleanup/reset does not leak; headless test target has no GPU/editor/model dependency.  
**Docs:** explain the handle encoding (index/generation bits, reuse and stale detection) in a short markdown file next to the code, per the code-style rule in CLAUDE.md.  
**Next slice:** P3.2 hierarchy then P3.3 draw-list extraction, not a training dashboard.

### Near-term order

B0 → P3.1 identity → P3.2 transforms → P3.3 draw lists → P3.4 resources → P3.5 derived assets → P3.6 persistence/validation → P3.7 reload → P3.8 evidence. Adjust this order only where actual dependencies or completed work justify it; document the change.

After core contracts settle, run W0 narrowly and proceed to P4. Start tiny actor/item/skill rule fixtures when P3 ownership makes them useful; do not block world work on unselected camera/hardware/ML details. Bring P8A forward only after the relevant turn-based rules can run headlessly, and keep it independently runnable from the normal build.

## 13. Decision and risk registers

### 13.1 Resolved product decisions

| ID | Resolution | Requirement |
|---|---|---|
| D1 (prior baseline) | Both modes are independently usable and optionally integrable; reference game includes both | C16, user 21 |
| D2 (prior baseline) | First learned policy is turn-based | C17, user 22 |
| D3 (prior baseline) | Aggression at lower player HP is a learned preference, not mandatory rule | C18, user 23 |

Do not reopen these questions absent a new user instruction. Do not treat a technical workaround that violates them as an equivalent implementation.

### 13.2 Open engineering decisions

These are bounded decisions, not a request to stop and ask the user all of them now. Resolve reversible details at their milestone with a short ADR/evidence record. Product-facing changes and materially costly commitments need explicit approval.

| ID | Decision / default | Resolve before | Revisit trigger |
|---|---|---|---|
| O01 | World storage/ID encoding; simple generational ownership before a general ECS | P3.1 | Measured scale/query limitation |
| O02 | Source format, serializer, descriptor approach, artifact version/hash design. **P3.5 (2026-11-01):** models stay glTF; derived `.umodel` is a custom versioned binary with FNV-1a source+dependency hash (see `engine/assets/include/AssetPipeline.md`). **P3.6:** scenes are versioned JSON (`*.scene.json`, nlohmann-json, already a tinygltf dependency) with string durable ids; see `engine/scene/include/SceneFormat.md`. | P3.5/P3.6 | Tool interop or measured pipeline issue |
| O03 | Camera/controls/art and minimal animation approach. **User decision 2026-11-06:** third-person orbit camera; procedural animation only in P4 (no skeletal system yet). | P4.2/P4.3 | User reference-game direction changes |
| O04 | Jolt integration suitability; retain original intent pending spike. **P4.1 (2026-11-06, user-approved):** Jolt 5.6 (vcpkg `joltphysics`) behind `Umbra::Physics`; single-threaded job system; boxes/spheres/capsules + `CharacterVirtual`; see `engine/physics/include/Physics.md`. | P4.1 | Platform/build/behavior mismatch |
| O05 | Turn order/AP, real-time cost timing, effect clocks and mode integration. **User decisions 2026-11-07:** speed-ordered rounds, one action per fighter; real-time cost paid at commit (end of wind-up), interruption during wind-up is free, no refund after commit; statuses and cooldowns end with the encounter and modes change only between encounters. | P5 controllers | A concrete mechanic requires another rule |
| O06 | Enemy instance/archetype meaning; reference encounter arrangement. **P5 default:** an actor definition is a reusable archetype. **User decisions 2026-11-07:** dungeon areas have enemy pools; enemies roam (some chase on sight) and touching one starts a fight with it plus up to two more from the pool (1–3 enemies); defeated roamers return when the area is re-entered. Fights happen in place (superseded later on 2026-11-07 after the first play test: every fight, turn-based and real-time, moves to a separate battle arena scene and returns to the same dungeon spot). The player picks turn-based (default) or real-time outside combat; it can't change during a fight. Fleeing always works. | P5/P6 content | User content scope clarification |
| O07 | Overflow, respec, quest failure, save/retry UX. **User decisions 2026-11-07:** a full inventory rejects the pickup; quests can fail (data-driven, e.g. on the player's death); saving at fixed save stones (E to interact, one manual slot with a backup) and automatically on area change (one autosave slot); on death the player picks "load save" or "load last autosave"; growth by XP and levels; placeholder quest written by Claude. Respec: none yet (default). | P5/P6 | Reference walkthrough softlock or usability finding |
| O08 | Training tools/algorithm/hardware; external Python is proposed default. **Decided 2026-11-07:** Python + PyTorch on CPU, PPO with action masks, through a C bridge to the native rules (user: train in Claude's workspace). | P8A.2 | Inadequate results or workflow/deployment cost |
| O09 | Reward/style objective and held-out evaluation thresholds | Before final P8A evaluation | Evidence shows confounding or reward exploit |
| O10 | Inference runtime/operators/device and budget; candidate CPU ONNX path. **Decided 2026-11-07 (user):** built-in C++ MLP, no ONNX Runtime; 3.9 µs per decision. | P8A.4 | Actual policy latency/resource/platform issue |
| O11 | Local authoring provider/model/license and novice installation experience. **Decided 2026-11-07 (user):** Ollama on the creator's PC, default `qwen2.5:7b` (4.7 GB, Apache-2.0), installed by the creator, not shipped; editor window + `UmbraTool draft`; drafts only add one quest and its conversation lines and only reference existing content. Model quality on real drafts is still unmeasured. | P8B.1 | Model quality, resource or redistribution problem |
| O12 | Reference PC/settings/workload and quantitative release budgets | Before P9 sign-off | Content or target tier changes |
| O19 | Web export scope. **Decided 2026-11-08 (user):** supported browsers Chrome + Edge (WebGPU); hosting undecided → single-threaded build for any static host; toolchain: Emscripten in the cloud (3.1.52 with the system LLVM 18). | P10 | A target host or browser that this excludes |
| O13 | Browser backend, shader path, capabilities/hosting support. **User decision 2026-11-06:** run W0 after P4. **W0 (2026-11-07):** WebGPU for graphics (separate small backend behind `core::DrawList`), HLSL → SPIR-V → WGSL planned, Emscripten candidate for P10; rules already build to WebAssembly unchanged. See `docs/adr/0001-web-backend.md`. | W0; finalize P10 | Actual supported-browser failure |
| O14 | First mobile OS/device tier | P11 planning | User target choice |
| O15 | Community records versus model updates, consent, backend operations | F1 approval only | Explicit decision to fund/build service |
| O18 | Reference workload and performance budget. **Decided 2026-11-08 (user confirmed after measuring):** reference PC = the user's PC (NVIDIA RTX 3050 Laptop GPU, 16 threads, 16 GB; measured 3.47 ms average / ≤ 6.2 ms p99); 1920×1080, default settings; workload = `Umbra --benchmark` (settlement, dungeon, arena) plus a learned-enemy fight; budget average ≤ 16.67 ms (60 FPS) and p99 ≤ 33.3 ms per area. | Before P9 sign-off | Measurements far from the budget either way, or a different target PC |
| O17 | AI 3D model generator, scope, budget. **Decided 2026-11-07 (user, ADR 0003):** ComfyUI image-to-3D with TRELLIS.2 (MIT) through the creator's exported workflow; props first (characters stay with Meshy); ~10 000 triangles; text → SDXL concept or own picture. | Before P8D.0 | Quality/VRAM unusable on the user's PC |
| O16 | AI texture generator, default model, scope and timing. **Decided 2026-11-07 (user, ADR 0002 §7):** ComfyUI first behind an `ImageProvider` interface (Ollama later); tileable world surfaces first; SDXL base 1.0 at 1024 px for the user's 8 GB GPU; independent track before P9. | Before P8C.0 | Generator unusable on the user's PC, or Ollama image generation reaches Windows |

### 13.3 Initial risk register

Risk ratings are planning judgments, not measured probabilities. Every listed risk has an early observable trigger and a containment path.

| Risk | Trigger / evidence to watch | Containment | Gate |
|---|---|---|---|
| R01 Scope growth from two modes | Duplicate systems or integration taking over every task | Common resolver, standalone fixtures, safe boundaries; no arbitrary mid-action switching | P5 |
| R02 Learning goal competes with finishing game | Long experiments produce no runnable milestone | Separate research executables, small ADRs, reference-game-first gates | All |
| R03 “AI-native” becomes framework bloat | Generic agent/plugin servers before working content commands | Shared thin validators/CLI; optional providers; no core AI dependency | P7/P8 |
| R04 Browser port discovered too late | Native-only APIs/shaders throughout world/gameplay | W0 experiment and narrow adapters; keep Vulkan working | P3/W0 |
| R05 Learned style never emerges | High reward but no matched-probe preference, or hard override | Objective/evaluation redesign; retain playable baseline without falsely closing P8A | P8A |
| R06 Simulator loophole or reset leak | Training wins that disappear in native gameplay | Shared native rules, reset/step parity, adversarial scenarios | P5/P8A |
| R07 Inference frame hitches | Worker count/queue/memory increases tail frame time | Bounded resources/deadline/provider disablement; measure whole game | P8A/P9 |
| R08 Reload/save corruption | Mixed content revision, dangling handle, interrupted write | Transactional staging, durable IDs, backups and migration tests | P3/P6 |
| R09 Export/runtime incompatibility | Matching shapes but wrong mapping/operators/results | Manifest semantics plus native and browser parity gates | P8A/P10 |
| R10 Creator experience requires expert ML knowledge | Sample fails without undocumented setup | One supported recipe, pinned tools, clear diagnostics and explicit limits | P7/P8 |
| R11 Hidden online dependency | Fresh install needs login/cache/model service | Clean offline package tests and no automatic network fallback | P6/P9 |
| R12 Future service undermines offline scope | Upload/account hooks appear before approval | Keep F1 deferred; separate adapter and consent design later | F1 |
| R13 Dependency/model distribution risk | Untracked license, provenance or heavy transitive dependency | Inventory/pins/review before adoption and packaging | Per dependency/P9 |
| R14 False progress from agent summaries | Dashboard says complete without commands/artifacts | Evidence states and source verification; manual checks separate | Every handoff |

## 14. Evidence and handoff templates

Use existing repository conventions first. These templates are fallback structures, not a demand to create four new tracking systems. Generated documents contain no completed evidence entries.

### 14.1 Task card

```text
Task ID:
Milestone / requirement IDs:
Intent and runnable artifact:
Prerequisite evidence:
Scope / explicit exclusions:
Files and public contracts affected:
Provisional assumptions / decision needed:
Implementation steps:
Normal, failure, cleanup and regression tests:
Manual/platform checks:
Definition of done:
```

### 14.2 Verification record

```text
Date and environment:
Repository revision and dirty-state description:
Task / gate / test-case IDs:
Compiler, build configuration, dependency baseline:
Commands actually executed:
Automated results and evidence paths:
Manual checks/captures actually performed:
Pre-existing failures:
Not-run checks and why:
Observed metrics (hardware/settings included):
State: planned | in progress | implemented/unverified | verified complete | blocked | deferred
Remaining risks and next task:
```

Never invent a commit hash, report a capture not taken or count skipped tests as passed. Preserve historical reports under their original date. State precisely whether evidence came from a user report, source inspection or a run in the current environment.

### 14.3 Decision record

```text
Decision ID and date:
Requirement vs engineering-default distinction:
Problem and constraints:
Options considered:
Experiment/evidence:
Chosen approach and scope:
Rejected alternatives and why:
Compatibility/migration/security implications:
Revisit trigger:
User approval reference, when required:
```

### 14.4 Model evaluation record

```text
Policy/model hash and contract versions:
Native simulator/ruleset/content revision:
Training toolchain/configuration/seeds/data split:
Observation/action/preprocessing/mask contract:
Baseline opponent and fallback behavior:
Declared evaluation thresholds and rationale:
Held-out competence and difficulty results:
Paired aggression probes, counterexamples and uncertainty:
Legality/fallback/timeout rates:
Export/native output and complete-encounter agreement:
Runtime hardware, worker limits, latency and memory:
Known failure scenarios / rejected candidate reason:
Promotion decision and permitted deployment profile:
```

A final session handoff should identify the actual completed slice, exact verification, remaining open gate and next action. Keep long logs in artifacts, not in the stable context file. Update C01–C18 only for a user decision; update dashboard evidence only for observed progress.

## 15. Sources and change history

### 15.1 Project-derived content

P1: `docs/custom-3d-game-engine-roadmap.md` — original learning-first plan and history (marked superseded for future sequencing).  
P2: `docs/current-engine-status.md`, status date 2026-09-02 — implementation baseline; partially re-checked by B0 on 2026-11-01.  
P3: `umbra-rpg-roadmap-baseline-2026-11-01.md` — previous proposed plan through user answers 1–20; source for the retained structure and T01–T38.  
P4: User answers 1–23 — authoritative scope decisions summarized in CONTEXT C01–C18.

The broader earlier engine survey is not treated as authoritative implementation evidence. Unsupported timetable, online, scripting and package suggestions from it are not inherited.

### 15.2 Current primary technical references

Consulted on 2026-11-01 for limited tool/interface facts. These links do not choose dependency versions or establish that Umbra supports their features. Before implementation, read the documentation for the actual pinned version. Most architecture, acceptance policies and task decomposition in this roadmap are design proposals derived from the project brief, not quotations from these sources.

- **S1 — Anthropic, Claude Code memory.** Project instruction files, concise guidance and on-demand context. `https://code.claude.com/docs/en/memory`
- **S2 — Farama Foundation, Gymnasium Env.** Reset/step contract and terminated/truncated episode outcomes. `https://gymnasium.farama.org/api/env/`
- **S3 — Stable Baselines3 Contrib, Maskable PPO.** Candidate action masking and mask-aware evaluation. `https://sb3-contrib.readthedocs.io/en/master/modules/ppo_mask.html`
- **S4 — ONNX Runtime C++.** Native inference integration candidate. `https://onnxruntime.ai/docs/get-started/with-cpp.html`
- **S5 — ONNX Runtime thread management.** Controls relevant to bounded inference resource use. `https://onnxruntime.ai/docs/performance/tune-performance/threading.html`
- **S6 — ONNX Runtime Web.** Browser integration and backend-dependent operator coverage. `https://onnxruntime.ai/docs/tutorials/web/`
- **S7 — ONNX Runtime model validation.** Application responsibility for fitness/performance and model resource risks. `https://onnxruntime.ai/docs/`
- **S8 — Emscripten WebGPU support.** Emdawnwebgpu browser integration path. `https://emscripten.org/docs/porting/multimedia_and_graphics/WebGPU-support.html`
- **S9 — Google web.dev, Service workers.** Cache-backed offline web application behavior. `https://web.dev/learn/pwa/service-workers`
- **S10 — Mozilla MDN, Storage quotas and eviction criteria.** Browser persistence limits and recovery considerations. `https://developer.mozilla.org/en-US/docs/Web/API/Storage_API/Storage_quotas_and_eviction_criteria`
- **S11 — Stable Baselines3 on-policy implementation.** Current-policy rollout collection; historical contributions are not automatically interchangeable. `https://stable-baselines3.readthedocs.io/en/master/_modules/stable_baselines3/common/on_policy_algorithm.html`
- **S12 — ggml-org, llama.cpp.** Local C/C++ language-model inference candidate for authoring tools only. `https://github.com/ggml-org/llama.cpp`

This reference list supports the limited factual technology statements, not a requirement to adopt every library.

### 15.3 Changes from the prior baseline

Version 1.0, 2026-11-01: incorporates final answers 21–23; makes independent combat-mode use an explicit contract/test; selects turn-based learned policy; removes mandatory-threshold interpretation of aggression; preserves deferred modding and offline scope; expands milestone tasks/evidence; retains T01–T38 and adds T39–T60; establishes concise root instructions plus separate context/roadmap files.

No implementation completion was advanced by preparing this pack. Further updates must record actual evidence, product changes and their source separately.

2026-11-08: O18 decided (user). P10 implemented (`#UMBRA-66`): Emscripten build of the game, WebGPU renderer in a JS library, HTML HUD, IndexedDB saves with export/import, offline service worker, no-WebGPU/device-lost messages, `scripts\serve-web.bat`, O19; evidence `testing/p10-web-export.md`.

2026-11-08: P9 benchmark measured on the user's PC (far inside the budget, no memory growth); `#UMBRA-65` start menu with Continue (a dungeon save seemed lost after restarting).

2026-11-08: P9 started (`#UMBRA-64`): benchmark mode, long-run soak test (T74), package check with license notices (T75), run-time asset lookup (no build paths in the game), O18 proposed; evidence `testing/p9-release-gate.md`.

2026-11-08: `#UMBRA-63` fitted colliders for models (user report: the collider of a generated object was far bigger than the object): placed models get a fitted static box; a new collider on a model entity fits it; collider sizes editable, **Fit to model**; the editor's selection box uses the mesh bounds and the collider is outlined; T73.

2026-11-08: the user verified P8B, P8C and P8D on Windows (works, with limitations). `#UMBRA-62` fixes the P8D limitations: any exported workflow name (remembered), the textured GLB from the save node (graph reduced to what it needs), jobs survive closing the editor (`--resume` in UmbraTool), no total time limit; T71, T72.

2026-11-07: P8D (AI 3D models through ComfyUI) implemented at the user's request (`#UMBRA-61`): row, section, status, T67–T70, O17, ADR 0003; evidence `testing/p8d-ai-models.md`. `#UMBRA-60`: scripts treat any non-zero exit code as failure.

2026-11-07: P8C implemented (`#UMBRA-59`), status block added; evidence `testing/p8c-ai-textures.md`.

2026-11-07: P8C (AI texture generation) planned at the user's request: dashboard row, milestone section, T61–T66, O16, ADR 0002; user decisions: ComfyUI, world surfaces, SDXL (8 GB GPU), before P9.

2026-11-07: P8B implemented (`#UMBRA-55`); dashboard, P8B status, O11 updated; duplicate stale P8A dashboard row removed; evidence in `testing/p8b-ai-authoring.md`.

Version 1.2, 2026-11-01: Phase 3 implemented (P3.1–P3.8); dashboard, task status, B0 findings and repository layout updated; evidence in `testing/phase3-assets-world.md`.

Version 1.1, 2026-11-01: adapted to the repository. Files moved to `docs/` with corrected links. Dashboard updated from a partial B0: B0 in progress, P3 in progress (uncommitted World stub), Phase 1 checklist gap noted. B0 findings table added. Real module, folder and test names recorded. Evidence location set to `docs/current-engine-status.md` + `docs/testing/`. Project code-style rule (short code, step comments, markdown beside code for deep topics) added. No requirement (C01–C18) changed.
