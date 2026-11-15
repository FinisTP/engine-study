# Umbra Engine - Current Development Status

**Status date:** 2026-11-08 (Phase 1/2 records dated 2026-09-02)
**Current milestone:** The Last Curtain, the fifth feedback implemented (2026-11-16, `#UMBRA-214`-`#UMBRA-216`; scenes on the board; turn banners and order; the Taken's HP and a staged fall; dialogs clickable anywhere (engine UI); the light switch, off for testing; nine Stable Audio 3 sounds; Windows check open, `testing/s37-fifth-feedback.md`). The Last Curtain, floor plaques, the Saigon sky and guest pages implemented (2026-11-15, `#UMBRA-211`-`#UMBRA-213`; room names on brass plaques lying on each floor; a painted sky behind the board, dusk before midnight, a red moon after; portraits on the start screen and a guest page with Start and Back; Windows check open, `testing/s36-plaques-sky-guest-pages.md`). The Last Curtain, the user's third feedback implemented (2026-11-15, `#UMBRA-192`-`#UMBRA-199`; the room view is a play mode with a zoom between views; moves count as the shortest way back to where the turn began; bots' deeds previewed; visible fights; chat replies; markers; readable walls; save version 4; Windows check open, `testing/s34-third-feedback.md`). The Last Curtain plan 4 and the user's feedback implemented (2026-11-15, `#UMBRA-170`-`#UMBRA-188`; 134 SDXL pictures painted on the user's PC, 14 tried again; the mood from a creep to blood in shaders; Look and battles at full size with the eight rooms around; persistent events and traps with clear conditions; turn arrow, room panel, tap to continue, Skip, `options.csv`; a room plays step by step; UI clicks reach any screen's buttons unless a dimmed one is above; Linux GCC/Clang/ASan; Windows look and play checks open, `testing/s30`-`s33`). The Last Curtain plan 3, the model's lines, implemented (2026-11-15, `#UMBRA-162`-`#UMBRA-169`; a voice per guest; the nightmind module's requests and guard; Words on a worker thread; Play waits up to 3 s for a line (user), else the written one; `models/night.gguf`; `UmbraTool night-mind`; stand-in model only on Linux; the real-model check on Windows open, `testing/s29-model-lines.md` §4; next: plan 4a, the painted table). The Last Curtain guest colours and pawn figures implemented (2026-11-15, `#UMBRA-158`-`#UMBRA-160`; a `color` per guest from the 1960s palette; one tinted, animated guest model and one shared Taken model from Meshy, 134 credits; Linux GCC/Clang/ASan 55/55; Windows look check open, `testing/s28b-pawn-figures.md` §4); plan 3 and plan 4 drafted for the user's review (`design/2026-11-15-*.md`). The Last Curtain plan 2c, the screen, implemented (2026-11-15, `#UMBRA-146`-`#UMBRA-156`; no ally goals (user); walls wherever a guest cannot pass; a turn menu, walking clicks one sliding step at a time, Stay/Back at doors; input by state, inspect, a save menu; bots think max(0.4 s, decision time) and slide; the camera follows, drag and Focus; labels and numbered group rings; Linux GCC/Clang/ASan 55/55; the Windows play check open, `testing/s28-the-screen.md` §4; next: plan 3, the model's lines). The Last Curtain plan 2b, goals and a bigger board, implemented (2026-11-15, `#UMBRA-137`-`#UMBRA-145`; omens removed; each guest draws one of two or three goals, the Taken is drawn from those who failed, the others drop to 1 HP, all done means no hunt; a 9×9 board of 70 rooms, seven start rooms; a free step back without pacing; chats and groups between any guests; bots with a purpose; the soak checks every band and holds on seeds 1, 20001, 80001; save version 2; Linux GCC/Clang/ASan 55/55; Windows check open, `testing/s27-goals-and-board.md` §4; next: plan 2c, the screen). The Last Curtain plan 2, the board on screen, implemented (2026-11-14, `#UMBRA-121`-`#UMBRA-131`; `LastCurtain.exe`: a table view, the player's turn, paced bot turns with captions, chat, event, dice, omen, battle, Turn and end screens, saves; the conversation rule and push back in the rules; Linux GCC/Clang/ASan 55/55, SwiftShader pictures; Windows and play check open, `testing/s26-board-on-screen.md` §4; next: plan 3, the model's lines). The Last Curtain plan 1, the night as a simulation, implemented (2026-11-14, `#UMBRA-104`-`#UMBRA-115`; ADR 0012; `plugins/last-curtain`: rules, data, bots, soak test, tools; first balance pass holds, every Taken form 43-56% over 1,000-2,000 nights; Linux GCC 55/55; MSVC matched on 2026-11-14; revised in `#UMBRA-116`: the curse turns at once, no evening fights, minions; MSVC re-check open, `testing/s25-night-simulation.md` §5; next: plan 2, the board on screen). S24 storyteller & outcome plan implemented (2026-11-12, `#UMBRA-101`; T93; 1,000 seeds of the placeholder plot hold; MSVC and the user's check open; S25 next, theme and plot first). S23 built-in model implemented and measured (2026-11-12, `#UMBRA-100`; Qwen3 4B on the graphics card: 0.8 s a line, 0.15 s a pick, 14 of 15 accepted; warm-up added; in-game stutter check and packaging open; `testing/s23-built-in-model.md` §2g). S22 memory, chronicle & rumours implemented (2026-11-11, `#UMBRA-92`; T91; MSVC and the user's check open; S23 next, model choice first). S21 world tick & residents implemented (2026-11-10, `#UMBRA-91`; headless 53/53 GCC/Clang/ASan, T90; MSVC and the user's check open; S22 next). S8 and S9 reviewed by the user (2026-11-10; backward rigs repaired in `#UMBRA-90`). S9 animation events, SFX & VFX implemented (2026-11-10, `#UMBRA-86`; headless 52/52 GCC/Clang/ASan, T89, Vulkan and WebGPU pictures on SwiftShader; the user's check with a GPU and sound open; S21 next). S8 skeletal animation & rigging implemented (2026-11-10, `#UMBRA-84`; headless 52/52 GCC/Clang/ASan, T87, T88, Vulkan and WebGPU pictures on SwiftShader; the user's GPU check and a real UniRig/HY-Motion run open; S9 next). S6 and S7 verified by the user (2026-11-09 review); plan extended with S18 party, S19 previewers, S20 editor settings (`#UMBRA-84`); open: Game UI window undo crash. S7 post-processing & custom shaders implemented (2026-11-09, `#UMBRA-83`; headless 50/50 GCC/Clang/ASan, check-web 13/13, Vulkan and WebGPU pictures on SwiftShader; the user's S6+S7 check open); S6 lighting & atmosphere implemented (2026-11-09, `#UMBRA-82`; headless 49/49 GCC/Clang/ASan, plugin sets 37/37 and 33/33, check-web 13/13, Vulkan and WebGPU pictures on SwiftShader; GPU look and frame time on Windows open; S7 next); S5 agent prompt implemented (2026-11-08, ADR 0008, `#UMBRA-80`; headless 49/49 GCC/Clang/ASan; real-model task set and editor window on Windows open); P11 Android implemented (2026-11-08, ADR 0009; headless 48/48 GCC/Clang/ASan; Gradle/NDK build and emulator run on Windows open); S4 AI sound, music, turn menu and hotkeys implemented (2026-11-08, ADR 0007; headless 48/48 GCC/Clang/ASan, Windows check open); S3 gameplay UI framework implemented (ADR 0006; Windows check open); S1/S2 implemented (Windows checks open); P10 web export renders on the user's PC; P9 PC release gate: benchmark passed, remaining Windows runs open
**Layout:** `engine/` base engine, `game/` base game, `projects/reference/{assets,engine,game}` (content moved from `assets/`)
**Phase 1 status:** Complete (interactive checklist unsigned)
**Phase 2 status:** Complete
**Phase 3 status:** Implemented. Headless tests verified on Linux; Windows build and GPU checks not run yet
**Phase 4 status:** Implemented. Headless tests verified on Linux (18/18, GCC/Clang/ASan); Windows, GPU, audio and interactive checks not run yet
**Phase 5 status:** Implemented. Headless tests verified on Linux (24/24, GCC/Clang/ASan); Windows build/CTest not run yet; wired into the game in P6
**Phase 6 status:** Implemented. Headless tests verified on Linux (27/27, GCC/Clang/ASan); Windows build 27/27 on 2026-11-07; first play test done (fixes in `#UMBRA-43`: save stone reach, separate battle arena, hit flash); full play-through open
**P8A status:** Implemented. One model accepted under declared thresholds (seed 301); the same recipe failed for seed 300; competence equals the conventional AI; Windows/in-game run open
**P8B status:** Implemented. Headless tests verified on Linux (40/40, GCC/Clang/ASan) against a stand-in Ollama server and saved drafts; no real model has run yet; the editor window compiles but has not been opened
**P8C status:** Implemented. Headless tests verified on Linux (42/42, GCC/Clang/ASan) against a stand-in ComfyUI; no real ComfyUI/SDXL or GPU run yet (P8C.0 open); editor window not opened
**P8B/P8C/P8D status:** Verified on Windows by the user (2026-11-08, works with limitations; P8D fixes `#UMBRA-62` awaiting re-check)
**Phase 7 status:** Implemented. The editor ran on Windows after fixes `#UMBRA-50`/`#UMBRA-51`. Headless tests verified on Linux (32/32, GCC/Clang/ASan); the editor compiles but has not been run; Windows checklist open

## Update 2026-11-10 (direction: the living town, ADR 0011, `#UMBRA-85`)

- The user chose the game's direction: an open-ended storytelling simulation RPG. A main plot that always wins, inside a simulated town of about seven residents that is the core gameplay; a required built-in local model that proposes and words (the simulation validates); outcomes that differ per playthrough and are managed by the developer through an outcome plan; theme open (need not be fantasy).
- Planned as S21-S25 after S9; S10 and S18 are re-scoped into them. A living-town game is PC only for now. Nothing is implemented yet.

## Update 2026-11-12 (S24 storyteller & outcome plan, `#UMBRA-101`)

- Town rules gained trouble: ill, theft, feud, leave, death (`plugins/living-town/modules/town/src/Trouble.cpp`, `Town.md` "Trouble"). The rules only propose; nothing goes wrong without a storyteller. Residents can be gone; trouble is remembered and talked about like any fact.
- `plugins/living-town/modules/story` (`Storyteller.md`): the outcome plan `town/*.plan.json` (beats with day windows and allowed outcomes, `never`, `allow`, pacing), checked on load; the storyteller acts once a day: filter, the waiting beat first, force on the window's last day (with a warning), else nudge by rehearsing each proposal in a forked town two days ahead.
- `UmbraTool town-soak <town> <plan>`: 1,000 playthroughs in about 3 s; the placeholder plot (theft, row, someone gone) always finishes, nothing against the plan, 69 different roads, 4-10% of beats forced against 19-29% without rehearsal.
- Game: the plan beside the town file is loaded, the story is saved with the town, the gone are not shown. Editor: Town plot window. Residents have an optional `pronoun`; the model's prompt names it and a tenth guard check refuses a wrong "he"/"she".
- S23 addendum (`#UMBRA-100`): Qwen3 4B on the user's graphics card: 0.8 s a line, 0.15 s a pick; `Mind::WarmUp` moves the 18.5 s first-use cost to the worker thread at start.
- User decisions 2026-11-12: death allowed where the plan says; nudge, then force; placeholder plot; optional pronoun.
- Verified: T93 on GCC, Clang, ASan. Not verified: MSVC, the editor window (compiled only), trouble seen in the running game. Limits: beats are town trouble only (no quests or player actions yet), one plot line, five kinds fixed in code.

## Update 2026-11-11 (S23 built-in model, `#UMBRA-93`)

- `plugins/living-town/modules/mind` (`Mind.md`): the model only proposes. It words the news a resident tells (asked when the player comes near, never waited for) and picks among legal activities; a grammar fixes the shape of every answer, the line guard (five checks, extendable; `town/lines.rules.json`) the content, `Legal` every pick. Refused, late or missing answers fall back to templates and rules, leaving the town's state as without a model.
- What was said is stored in the town's state and the save; the chronicle shows those words; a loaded game does not ask again.
- llama.cpp b9030 through vcpkg (`llama-cpp`, feature `town-model`, on in the `windows-msvc` preset); the game loads `projects/<project>/models/town.gguf` if present. `UmbraTool town-mind` measures a model file.
- User decisions 2026-11-11: model up to 4-5 GB, user downloads it, English only, on-fact lines with extendable filters, residents walking left for S25.
- Verified: T92 with a scripted stand-in on GCC, Clang, ASan; the llama.cpp path with a random-weights model in the cloud. Not verified: any real model, MSVC, vcpkg's llama-cpp on Windows, packaging (`testing/s23-built-in-model.md` §3).

## Update 2026-11-11 (S22 memory, chronicle & rumours, `#UMBRA-92`)

- The town remembers: notable events become facts with three sets of residents (who saw, who knows, who told the player). On every talk each side passes on the newest fact the other lacks, so news travels along the same meetings that make friendships.
- The player enters the town: residents can be talked to (**E**), ask favours (go and see someone in time), thank, pass on news in template sentences, and greet by what they think of the player; that regard changes when they learn of a kept or skipped favour. The chronicle screen (**L**) lists what the player was told.
- The user's MSVC run of S21 printed the same 30-day fingerprint as GCC (2026-11-11): the town is identical across compilers.
- Verified: T91 on GCC, Clang, ASan; the screen through `ui-preview`. Not verified: MSVC for S22, play (`testing/s22-memory-chronicle-rumours.md` §3).

## Update 2026-11-10 (S21 world tick & residents, `#UMBRA-91`)

- New plugin `living-town` with the headless module `Umbra::Town` (`plugins/living-town/modules/town/include/Town.md`): a town file names needs, places, activities and residents; the state is one copyable value advanced in 10-minute ticks with whole-number rules and its own random stream; every change of activity is a validated intent; the rule-based chooser follows schedules and deals with pressing needs first.
- Forks: copying the state and running it ahead gives exactly what the real town will do (the base for S24's look-ahead). Saves: the town travels in the player's save (`SaveData::extras`); a save from a changed town file starts a new town.
- Game: `TownFlow` keeps the town in step with the world's time of day and shows residents at their places' markers (peaceful bodies; they vanish and appear when the town moves them). `UmbraTool town-run` lives N days headless.
- Example: seven residents in the settlement (`town/settlement.town.json`, seven `resident_*` actors).
- Also on 2026-11-10: `#UMBRA-87`/`#UMBRA-88` Visual Studio presets and editor launch entries; `#UMBRA-89` joint limits when packing clips; `#UMBRA-90` the hero's and bandit's rigs had been fitted backward by Meshy (models uploaded facing -Z) and were repaired with `pack_animated.py --rig-reversed`.
- Verified: T90 on GCC, Clang, ASan (golden fingerprint), about 9,000 town-days per second; a software-rendered picture with residents. Not verified: MSVC, real play (`testing/s21-world-tick-residents.md` §4).

## Update 2026-11-10 (S9 animation events, SFX & VFX, `#UMBRA-86`)

- Clip events: a state's `events` may name a `sound` and an `effect`; each fires once when the clip's time passes it, also across a slow frame and on every lap of a loop (`engine/scene/include/AnimGraph.md`). A trigger set just before a slow frame is no longer lost.
- Particles: `core::ParticleSystem` simulates effects from `vfx/*.vfx.json` on the CPU, repeatable per seed; the renderers draw them as instanced camera-facing squares with one premultiplied blend for glow and cover, last in the HDR scene pass, on Vulkan and WebGPU (`engine/core/include/Particles.md`). The `vfx` component keeps an effect running at an entity.
- Game: blows show when the attacker's `hit` event fires (sound, sparks or the skill's effect, flash, number); heavy blows freeze the game for 0.07 s and shake the camera; a hit on the player flashes red (`apps/umbra/GameFeel.md`, `GameFlow.md` "Blows"). Skills, items and statuses name an `effect`; validation loads every effect named by scenes and RPG content.
- Example: 13 burning torches, step dust with timed footsteps, swing and hit moments for the hero, bandit and wolf, a fireball skill, poison bubbles, a potion glow. `--demo <skill>` plays a fight by itself for pictures.
- Verified: 52/52 on GCC, Clang and ASan; plugin sets 34/34 and 38/38; web 13/13; validation layer clean; pictures in `docs/testing/s9/`. Not verified: sound and feel on real hardware, the editor view (user checklist, `testing/s9-events-sfx-vfx.md` §4).

## Update 2026-11-10 (S8 skeletal animation & rigging, `#UMBRA-84`)

- Skins and clips: the glTF importer keeps one skin per model (joints, inverse bind matrices, rest pose, per-vertex joints and normalised weights) and every clip by joint name; the `.umodel` cache stores them (format 2). Math in `engine/core/include/Skeleton.md`.
- Animation: `core::Animator` plays a graph (`animations/*.anim.json`, `engine/scene/include/AnimGraph.md`): the first matching state wins (flag, number above a value, trigger for one-shots), states crossfade, walk cycles follow the speed. Entities get it through the `animation` component; RPG actors through their `animation` field. Rigged models without a graph hold their rest pose.
- Rendering: GPU skinning on Vulkan (joint storage buffer per frame, skinned scene/shadow/surface pipelines) and WebGPU (the same in WGSL). Validation layer clean on SwiftShader.
- Game: the hero, bandits and wolves animate from their speed; attacks, hits and deaths in fights play their clips; a defeated enemy falls before it disappears. The example characters were re-made: the hero and bandit rigged and animated with Meshy (34 credits), the wolf from Quaternius (CC0); the elder stays unrigged (Meshy could not rig her).
- AI: the editor's AI animation window, `UmbraTool rig` and `UmbraTool motion` run the creator's exported UniRig / HY-Motion ComfyUI workflows (`plugins/ai-authoring/modules/localai/include/AnimationGen.md`); FBX results are converted with Blender. Tested with a stand-in ComfyUI only.
- Also in `#UMBRA-84`: crash dumps (`crash-<time>.dmp`) for the open Game UI undo crash; S18-S20 added to the plan.
- Evidence: `docs/testing/s8-skeletal-animation.md`. Open: the user's check on Windows (§4).

## Update 2026-11-09 (S7 post-processing & custom shaders, `#UMBRA-83`)

HDR scene picture, bloom (6 levels; 4 from quarter size on Android), ACES tonemap, a baked 16³
color table (LUT image + temperature/saturation/contrast), vignette; `post` component. Custom
material surface shaders (`*.surface.hlsl` inserted into the scene shader; Vulkan pipeline per
shader; WGSL for the browser by DXC + naga at build time; errors at the shader's line). Also fixed:
the install step now ships every compiled shader (S6's sky and shadow shaders were missing from
packages). Example: warm settlement with well water, cold dungeon with a LUT and a pulsing crystal.
Evidence: `docs/testing/s7-post-and-shaders.md`; math: `engine/core/include/Post.md`.

## Update 2026-11-09 (S6 lighting & atmosphere, `#UMBRA-82`)

Linear lighting (sRGB textures and screen), sky/ground ambient, sun or moon with a 2048² PCF shadow
map, up to 16 flickering point lights, emissive surfaces, exponential fog and a sky; `light` and
`environment` components (schemas in code); a day clock that carries across areas and into saves;
editor color pickers and light range rings; `Umbra --capture/--hour`. Same passes in WGSL for the
browser. Example: settlement at dusk with 7 torches, torch-lit dungeon with a glowing crystal.
Evidence: `docs/testing/s6-lighting.md`; math: `engine/core/include/Lighting.md`.

## Plan 2026-11-08 (S6-S17, `#UMBRA-81`)

- ADR 0010 plans twelve phases chosen by the user: RPG flair first (S6 lighting, S7 post-processing and custom shaders, S8 skeletal animation and AI rigging, S9 animation events/SFX/VFX, S10 RPG systems, S11 cutscenes), then the editor (S12 prefabs, S13 play in editor, S14 graph editors), then the agent (S15 chat and vision, S16 recipes and scatter, S17 code agents). Each ships an example. Nothing implemented yet.

## Update 2026-11-08 (S5 agent prompt, `#UMBRA-80`)

- `Umbra::Agent` (`plugins/ai-authoring/modules/agent`): 20 tools (read scenes/assets/components/RPG; change entities, components, RPG entries; generate textures, sounds, models) with JSON schemas, working-copy `Edits` validated by the game's loaders and committed as one undo step per document; `Planner` (local Ollama, schema-forced `look`/`steps` answer, dry run, fix rounds) and `Runner` (generators on a worker thread, stop at the first failure). Editor **Agent** window; `UmbraTool agent`; `UmbraTool mcp` (MCP stdio server, saved at once, optional build/tests). The ComfyUI generation pipelines moved into `localai::CreateTexture/Sound/Model`, shared with `UmbraTool texture/sound/model`. The editor reloads a scene changed on disk when nothing is unsaved. 49/49 GCC/Clang/ASan, plugin sets 37/37 and 33/33, 15/15 WebAssembly. Real-model task set on Windows open (`testing/s5-agent-prompt.md`).

## Update 2026-11-08 (P11 Android, `#UMBRA-79`)

- The game builds for Android as `libmain.so` inside a Gradle app (`plugins/android-export`: SDL 3.4.14's Java activity, a game-only vcpkg manifest, `scripts\build-android.bat` which packs, builds, starts the emulator and installs). Assets and shaders travel in `game.pack` (`UmbraTool game-pack`) and are unpacked once per version; shaders are now built for Vulkan 1.1. Going to the background saves, pauses and releases the Vulkan surface; returning makes a new one. Touch controls (floating stick, camera drag, buttons, fight hotkeys) and gamepad sticks (`apps/umbra/Mobile.md`). 48/48 GCC/Clang/ASan, plugin sets 37/37 and 33/33, 15/15 WebAssembly, check-web 13/13. The Android build and the emulator run on Windows are open (`testing/p11-android.md`).

## Update 2026-11-08 (S4 AI sound, music, turn menu, hotkeys, `#UMBRA-77`)

- AI music (ACE-Step 1.5) and sound effects (Stable Audio 3) through the local ComfyUI: looped/trimmed, normalized, saved as `.ogg` / `.wav` with provenance; editor **AI sound** window and `UmbraTool sound` (ADR 0007). The game streams Ogg music (stb_vorbis) per scene through a data-only Music component, crossfading on scene changes (the arena's music is the battle music); skills and items can name a sound. Turn fights got an Attack / Item / Flee menu with target arrows over the enemies; real-time fights use hotkeys 1-4 set on a new screen and kept in saves. 48/48 GCC/Clang/ASan, 15/15 WebAssembly, check-web 13/13; real-model runs and listening on Windows open (`testing/s4-sound-and-menus.md`).

## Update 2026-11-08 (S3 gameplay UI framework, `#UMBRA-75`)

- The game draws its own UI (`engine/ui`, ADR 0006): screens are `ui/*.ui.json` files with a skin and TTF fonts, bound to one JSON data object from `GameFlow`, drawn by Vulkan and WebGPU alike. All game UI is replaced (HUD, start, pause, death, conversation, fight) and inventory/equipment, skills and quest log are new; they live in the `rpg-ui` plugin. Keyboard, mouse and gamepad focus. Editor **Game UI** window and `UmbraTool ui-preview` (CPU-rendered PNG). 47/47 GCC/Clang/ASan, 14/14 WebAssembly suites, check-web 13/13. GPU, gamepad and editor checks on Windows open (`testing/s3-game-ui.md`).

## Update 2026-11-08 (P10 web export, `#UMBRA-66`)

- The game builds for the browser with Emscripten (single-threaded, WebGPU through a JS library behind the same `Renderer` interface, HUD as HTML, saves in IndexedDB with export/import, offline service worker). 13 headless suites pass in WebAssembly; 9/9 browser checks in headless Chrome. Rendering not yet seen on real hardware (`testing/p10-web-export.md`). O18 (performance target) and O19 (Chrome + Edge, single-thread) decided by the user.

## Update 2026-11-08 (P9 benchmark, `#UMBRA-65`)

- Benchmark on the user's PC (RTX 3050 Laptop, 1920×1080): 3.47 ms average (288 FPS), p99 ≤ 6.2 ms in all three areas, peak memory 250 MB with no growth (`testing/p9-release-gate.md` §3b).
- `#UMBRA-65`: a start menu at launch (Continue / Load the other save / New game) when a save exists; before, restarting always began fresh in the settlement, so a dungeon save seemed lost.

## Update 2026-11-08 (P9 started, `#UMBRA-64`)

- P9 (PC release gate) tools: `Umbra --benchmark` (frame-time percentiles, hardware, peak memory → `benchmark.json`), a 300-fight long-run test with save round trips and damaged-save rescue (T74), `UmbraTool package-check` run by `package.bat` (T75), `THIRD_PARTY_NOTICES.md` and vcpkg license texts in the package, and run-time asset lookup so the game no longer contains the source assets path. 44/44 CTest on GCC/Clang/ASan. The gate runs on Windows are open (`testing/p9-release-gate.md` §4); performance budget O18 proposed.

## Update 2026-11-08 (`#UMBRA-63`)

- Colliders for models: placed generated models get a static box fitted to their bounds; a collider added to a model entity in the inspector is fitted too (before, its size came from the entity scale: a 1 m model at scale 5 got a 10 m box); sizes are editable, **Fit to model** refits; the selection box shows the mesh bounds and the collider is outlined in green. 43/43 on GCC/Clang/ASan; editor not run.

## Update 2026-11-08 (Windows verification, `#UMBRA-62`)

- The user verified P8B (real Ollama), P8C (ComfyUI/SDXL) and P8D (TRELLIS.2 on an 8 GB GPU) on Windows: works, with limitations.
- `#UMBRA-62` fixes the P8D limitations: any exported workflow name (listed and remembered); the textured GLB from the save node (the graph is reduced to what that node needs, so previews and the unused Pixal3D branch don't run); a model job survives closing the editor (fetched on the next start; `UmbraTool model --resume`); no total time limit while ComfyUI still has the job. 43/43 on GCC/Clang/ASan; evidence `testing/p8d-ai-models.md` §3c.

## Update 2026-11-07 (P8D)

- P8D (`#UMBRA-61`): props from text (SDXL concepts) or a picture through the creator's exported TRELLIS.2 workflow in ComfyUI; GLB saved with concept and provenance, checked by the importer, placed with undo; editor AI model window, `UmbraTool model`. ADR 0003, evidence [`testing/p8d-ai-models.md`](testing/p8d-ai-models.md).
- `#UMBRA-60`: build/package/editor scripts treat any non-zero exit code as a failure (a blocked linker returned -1 and was reported as BUILD OK).

## Update 2026-11-07 (P8C)

- P8C (`#UMBRA-59`): materials (`*.material.json`, scene `material` field) with world-space texture coordinates and mipmaps for every texture; `localai` ComfyUI bridge; `assets::MakeSeamless`/`SeamScore`; provenance and license warnings (`UmbraTool validate`, `package.bat`); editor AI texture window with in-scene preview; `UmbraTool texture`; sample brick material on the settlement. Plan ADR 0002, evidence [`testing/p8c-ai-textures.md`](testing/p8c-ai-textures.md).
- Later P8B fixes: `#UMBRA-56` (menu button, installed-model picker, thinking off), `#UMBRA-57` (localhost dialed as 127.0.0.1).

## Update 2026-11-07 (P8B)

- P8B (`#UMBRA-55`): `Umbra::LocalAi` (local-only Ollama bridge: streamed, JSON-schema output, limits, instant cancel, background job), `authoring::PlanDraft` (draft → additive changes, game loader, play-through with the real `GameSession`), editor window View → AI quest draft, `UmbraTool draft`. New dependency: cpp-httplib (vcpkg, no default features). Evidence: [`testing/p8b-ai-authoring.md`](testing/p8b-ai-authoring.md).
- User decisions: default model `qwen2.5:7b`; editor window + command line; drafts only add one quest and its conversation lines, using existing content.

## Update 2026-11-07 (late night)

- Meshy characters (`#UMBRA-52`): hero, Elder Mara, wolf, bandit (bandit leader = tinted bandit); actors gained `model`, `size`, `color`.
- P8A (`#UMBRA-53`, `#UMBRA-54`): `Umbra::Policy`, `tools/policy` (PPO through a C bridge), `assets/policies/bandit_leader.policy.json`, game integration with fallback. Evidence: [`testing/p8a-learned-enemy.md`](testing/p8a-learned-enemy.md).
- User decisions: P8A first; built-in C++ network; training on CPU in Claude's workspace; P8B will use Ollama.

## Update 2026-11-07 (night)

- Phase 7 (P7.1–P7.6) implemented in commits `#UMBRA-44` … `#UMBRA-49`. Evidence: [`testing/phase7-creator-workflow.md`](testing/phase7-creator-workflow.md). Guide: [`creator-guide.md`](creator-guide.md).
- User decisions: separate editor program; number fields + ImGuizmo gizmo; forms for RPG content; package = folder + zip.
- New: `Umbra::Authoring` (validated documents with undo), `UmbraTool`, `UmbraEditor`, live RPG content reload in the game, `scripts/package.bat`, `scripts/editor.bat`, known-good build copy in `build-and-run.bat`.
- New dependencies: `imgui[sdl3-binding]`, `imguizmo` (vcpkg builds them on the next configure).

## Update 2026-11-07 (evening)

- Phase 6 (P6.1–P6.5) implemented in commits `#UMBRA-37` … `#UMBRA-42`. Evidence: [`testing/phase6-reference-game.md`](testing/phase6-reference-game.md).
- New modules: `Umbra::Story` (quests, conversations), `Umbra::Game` (saves, `GameSession`); `gameplay::Roamer`; app layer `apps/umbra/GameFlow`.
- Play: talk to Elder Mara (E), save at save stones (E), enemies roam the dungeon and touching one starts a fight in a separate battle arena, then returns to the same spot; Tab switches turn-based/real-time outside fights; real-time keys 1–4, Q, F; death menu loads the save or autosave.

## Update 2026-11-07 (afternoon)

- Windows: Claude built the MSVC Release tree on the user's PC (`scripts/build-and-run.bat`): 0 compiler warnings, 24/25 CTest pass, `Umbra.Assets` was blocked from starting by Windows (not a test failure), the game started. Details in the phase 3–5 testing records.
- W0 web experiment: the rules compile to WebAssembly unchanged and match native results exactly; in Microsoft Edge on the user's PC the same page also rendered the settlement with WebGPU and kept a value in localStorage across reloads. `samples/web`, `docs/adr/0001-web-backend.md`.

## Update 2026-11-07

- Phase 5 (P5.1–P5.8) implemented in commits `#UMBRA-26` … `#UMBRA-33`. Evidence: [`testing/phase5-rules-combat.md`](testing/phase5-rules-combat.md).
- User decisions: speed-ordered rounds, one action each; real-time cost paid at commit; statuses/cooldowns end with the encounter; full inventory rejects pickups. W0 moved after P5.

## Phase 5 completed work (implemented 2026-11-07)

| Area | What exists | Explained in |
|---|---|---|
| Content | `Umbra::Rpg`: versioned `umbra.rpg` JSON (statuses, skills, items, actor archetypes), validation, `assets/rpg/reference.rpg.json` | `game/rpg/include/Rpg.md` |
| State and rules | Characters (ids/counts), derived stats, inventory/equipment transactions, skill tree, shared action resolver | `game/rpg/include/Rpg.md` |
| Turn combat | `Umbra::TurnCombat`: speed rounds, conventional enemy, `TurnEnvironment` (reset/legal actions/step) | `game/turncombat/include/TurnCombat.md` |
| Real-time combat | `Umbra::RealtimeCombat`: wind-up/commit/active/recovery, cone hits, cooldowns, conventional enemy | `game/realtimecombat/include/RealtimeCombat.md` |
| Integration | `Umbra::Encounter`: one fight at a time, stale-decision rejection, shared progression | `game/encounter/include/Encounter.md` |

## Update 2026-11-06

- Phase 4 (P4.1–P4.5) implemented in commits `#UMBRA-21` … `#UMBRA-25`. Evidence and checklists: [`testing/phase4-sandbox.md`](testing/phase4-sandbox.md).
- User decisions: third-person orbit camera; Jolt behind `Umbra::Physics`; procedural animation only; W0 after P4.

## Phase 4 completed work (implemented 2026-11-06)

| Area | What exists | Explained in |
|---|---|---|
| Physics | `Umbra::Physics`: Jolt 5.6 adapter (bodies, raycasts, `CharacterVirtual`), fixed 60 Hz steps, collider scene field, debug outlines (F1) | `engine/physics/include/Physics.md`, `engine/render/src/DebugDraw.md` |
| Player | `gameplay::PlayerController`: camera-relative movement, sprint, jump, wall-aware orbit camera, interaction ray; mouse capture | `game/gameplay/include/PlayerController.md` |
| Presentation | Procedural bob/lean (`StrideAnimator`), `Umbra::Audio` (WAV, mixer, sound bank) + SDL audio device, interactables, missing-asset warnings; HUD and menus are game UI screens since S3 (`engine/ui/include/UI.md`) | `engine/audio/include/Audio.md`, `game/gameplay/include/StrideAnimator.md` |
| Areas | `portal` field and spawn markers, settlement ↔ dungeon, F5/F9 development checkpoint, R restart | `engine/scene/include/SceneFormat.md` |
| Observability | Rolling CPU section timings and resource counters in the overlay | [`testing/phase4-sandbox.md`](testing/phase4-sandbox.md) |

The app now starts in `assets/scenes/settlement.scene.json` with a third-person player; a gate leads to `dungeon.scene.json`.

## Update 2026-11-01

- Plan and requirements moved to [`ROADMAP.md`](ROADMAP.md) and [`CONTEXT.md`](CONTEXT.md); agent rules in [`../CLAUDE.md`](../CLAUDE.md).
- B0 baseline reconciliation is partly done: [`testing/b0-baseline-reconciliation.md`](testing/b0-baseline-reconciliation.md).
- Phase 3 (P3.1–P3.8) implemented in commits `#UMBRA-13` … `#UMBRA-20`. Evidence: [`testing/phase3-assets-world.md`](testing/phase3-assets-world.md).
- Phase 1 interactive checklist (`testing/phase1-smoke-test.md`) is still unsigned.

## Phase 3 completed work (implemented 2026-11-01)

| Area | What exists | Explained in |
|---|---|---|
| Identity | `core::SlotMap` / `Handle<Tag>`; `world::World` with string durable ids, per-world handle check | `engine/core/include/SlotMap.md`, `engine/world/include/World.md` |
| Hierarchy | Local TRS, parent/child links, cycle rejection, subtree destroy, dirty propagation | `engine/world/include/Transform.md` |
| Draw lists | `world::Renderable` → `core::DrawList` → renderer; per-draw push constants, per-texture descriptor sets | `engine/render/shaders/Scene.hlsl` |
| GPU resources | Generational mesh/texture handles, create/replace/destroy, fallback texture, frame-serial retire queue | `engine/render/src/ResourceLifetime.md` |
| Import / derived assets | `Umbra::Assets`: full glTF node hierarchy, bounds-checked accessors, `.umodel` cache with source+dependency hash | `engine/assets/include/AssetPipeline.md` |
| Scenes | `Umbra::Scene`: versioned JSON scene files, all-or-nothing load, save, structured diagnostics, `UmbraSceneCheck` CLI | `engine/scene/include/SceneFormat.md` |
| Reload | File watcher with settle time; transactional model reload; scene swap on success | `engine/assets/include/Reload.md` |
| Gates | `Umbra.HeadlessDependencies` test; validator CTests on good/broken scenes | `cmake/UmbraHeadlessCheck.cmake` |

The app now loads `assets/scenes/sample.scene.json` (or a scene path given as the first argument): a ring of tinted cubes, a spinning parent with a child, and the `models/tower.gltf` hierarchy. Editing the scene, `tower.gltf` or `tower.bin` while it runs reloads them.

## Project summary

Umbra is a learning-first, modular 3D game engine written in C++20. The first product target is Windows x64, followed by Android ARM64 after the desktop runtime and renderer are stable.

Phase 2 established the first complete Vulkan scene path: a textured indexed glTF primitive, camera and transforms, depth testing, explicit resource ownership, validation-aware swapchain recreation, and an ImGui diagnostics overlay.

## Verification status

The user confirmed completion of the Phase 2 build, runtime, validation, resize, shutdown, and RenderDoc checks on 2026-09-02. The recorded checklist is [`testing/phase2-rendering-smoke-test.md`](testing/phase2-rendering-smoke-test.md).

The Phase 1 verification record remains in [`testing/phase1-smoke-test.md`](testing/phase1-smoke-test.md). The repository provides MSVC Debug/Release, clang-cl Debug, and MSVC AddressSanitizer presets through `CMakePresets.json`.

## Toolchain and dependencies

| Area | Current selection |
|---|---|
| Language | C++20 |
| Primary compiler | MSVC x64 |
| Verification compiler | clang-cl x64 |
| Build system | CMake with Ninja Multi-Config |
| Dependency manager | vcpkg manifest mode with pinned registry baseline |
| Platform/windowing | SDL3 |
| Logging | spdlog behind Umbra Diagnostics |
| Unit tests | Catch2 through CTest |
| Graphics API | Vulkan through Volk |
| GPU allocation | Vulkan Memory Allocator |
| Shader source | HLSL compiled to SPIR-V with DXC and checked by `spirv-val` |
| Scene interchange | glTF 2.0 through TinyGLTF (in Umbra::Assets); scenes as versioned JSON via nlohmann-json |
| Diagnostics UI | Dear ImGui with an Umbra-owned Vulkan renderer |
| AI authoring (editor/tool only) | Local Ollama (quests) and ComfyUI (textures, SDXL default) over HTTP via cpp-httplib (`Umbra::LocalAi`); models installed by the creator |
| Materials | `*.material.json` + PNG via stb_image (`Umbra::Assets`); world-space texture coordinates; mipmaps |

## Current architecture

```text
Umbra executable (apps/umbra: Main, ModelLibrary, SceneRuntime)
|- Umbra::Encounter  coordinator for both combat modes   -> TurnCombat, RealtimeCombat   (not linked by the app yet)
|- Umbra::TurnCombat turn controller, TurnEnvironment    -> Rpg
|- Umbra::RealtimeCombat real-time controller           -> Rpg
|- Umbra::Rpg        content, characters, inventory, skills, shared resolver -> Core, nlohmann-json
|- Umbra::Gameplay   player, physics bridge, interaction, areas -> World, Physics
|- Umbra::Ui         game screens: skin, fonts, layout, bindings, focus -> Core, Assets
|- Umbra::Physics    Jolt adapter (private dependency)  -> Core, Jolt
|- Umbra::Audio      WAV/Ogg (stb_vorbis), mixer with music streams, sound bank -> Core, Assets
|- Umbra::Scene      scene files, model instancing, checkpoints -> World, Assets, nlohmann-json
|- Umbra::Assets     glTF import, .umodel cache, watcher -> Core, tinygltf
|- Umbra::World      entities, hierarchy, renderables    -> Core
|- Umbra::Render     Vulkan renderer, draws a DrawList   -> Core, Diagnostics, Platform, SDL3, Volk, VMA, ImGui
|  |- SceneResources: generational mesh/texture handles, fallback texture, retire queue
|  |- Indexed HLSL scene pipeline: set 0 camera, set 1 texture, push constants per draw
|  `- Swapchain, depth, two frames in flight, ImGui diagnostics overlay
|- Umbra::Platform   SDL3 window, events, input, audio device -> Audio
|- Umbra::Diagnostics logging, crash handling            -> spdlog
`- Umbra::Core       SlotMap, DrawList, MeshData, RetireQueue, Diagnostic, clocks -> glm
UmbraSceneCheck      headless scene/model validator      -> Scene, Assets
```

Headless rule (checked by `Umbra.HeadlessDependencies`): Core, World, Assets, Scene, Physics, Gameplay, Audio, Rpg, TurnCombat, RealtimeCombat, Encounter and UmbraSceneCheck never link Render, Platform, SDL3, Vulkan or ImGui. `Umbra.TurnOnlyDependencies` / `Umbra.RealtimeOnlyDependencies` check that each combat mode builds without the other.

Long-lived device and scene resources survive resize. Swapchain images, their views, depth images, framebuffers, compatible pipelines, per-frame submission resources, and overlay resources are recreated when presentation changes.

## Phase 1 completed work

- Modular CMake targets and compiler presets
- SDL window, event pump, input, focus, suspend/resume, and termination contracts
- Monotonic clock and fixed-step accumulator
- Diagnostics, crash handlers, assertions, and deterministic failure formatting
- Catch2/CTest coverage, warning policies, AddressSanitizer configuration, and CI
- RAII ownership and idempotent shutdown across platform resources

## Phase 2 completed work

### Vulkan foundation and presentation

- Volk initialization, Vulkan instance ownership, optional validation layer, and debug messenger
- SDL Vulkan surface ownership
- Physical-device evaluation with graphics/presentation queues, swapchain support, and anisotropy requirements
- Logical-device, graphics-queue, and presentation-queue ownership
- Swapchain format, presentation mode, extent, image, and image-view ownership
- Recoverable `SUCCESS`, `SUBOPTIMAL`, and `OUT_OF_DATE` frame results
- Deferred recreation while the surface is minimized or otherwise zero-sized
- Explicit SDL minimized, restored, and pixel-size-change handling

### Resources and scene loading

- VMA allocator with RAII buffer and image wrappers
- Mapped staging buffers and device-preferred vertex/index buffers
- Texture upload barriers, buffer-to-image copy, image view, and sampler
- Controlled glTF importer for positions, normals, UVs, indices, one material factor, and one texture
- Stable engine-facing mesh and texture handles

### Scene pipeline

- HLSL scene and overlay shaders compiled to Vulkan 1.3 SPIR-V
- Indexed triangle-list drawing through `vkCmdDrawIndexed`
- Model, view, and right-handed zero-to-one projection matrices
- Movable camera and rotating scene model
- Descriptor-set layout, pipeline layout, descriptor pool, and per-frame descriptor sets
- Per-frame mapped uniform buffers to avoid CPU/GPU update races
- Supported depth-format selection, depth testing/writing, and one depth attachment per swapchain image
- Base-color texture sampling and simple directional Lambert lighting

### Frame execution and diagnostics

- Two frames in flight with reusable command buffers, image-available semaphores, and in-flight fences
- Per-swapchain-image render-finished semaphores and image/fence tracking
- Validation-clean acquire, record, submit, present, resize, minimize/restore, and shutdown flow
- ImGui overlay showing GPU identity, swapchain extent, frame timing, mesh count, texture count, and index count
- RenderDoc inspection of the textured indexed draw and overlay

## Phase 2 scope boundary

The current importer intentionally loads only the first primitive and one base-color texture from a controlled asset. The scene shader implements a basic material rather than the complete glTF metallic-roughness model. Textures currently use one mip level, and initialization uploads wait for the graphics queue.

These are intentional later-phase items rather than Phase 2 blockers:

- Full glTF node/scene traversal and transform hierarchy
- Versioned derived runtime assets and dependency tracking
- Generational resource handles and a general resource manager
- Mip generation, compression, asynchronous uploads, and streaming
- Draw lists, batching, instancing, culling, and LOD
- Metallic-roughness PBR, HDR, tone mapping, shadows, and IBL
- Render-graph and multi-pass scheduling
- GPU timestamp-based profiling

## Phase 3 scope boundary and next steps

All seven items of the old Phase 3 entry point are implemented (see the table above). Left out on purpose:

- Async uploads, mipmaps, texture compression, streaming. Uploads are synchronous; textures have one mip level.
- Materials beyond base color factor + texture. No PBR, normal maps, skinning or animation (the importer warns and skips them).
- Gameplay-state preservation across scene reloads (done in P4.4: hot reload keeps the player; F5/F9 checkpoints).
- Packaging: the app reads assets from the source tree (`UMBRA_ASSET_DIRECTORY`) and caches in the build tree. A relocatable package layout is a P6/P9 task (roadmap T43).

Next: the P9 gate runs on Windows (`testing/p9-release-gate.md` §4), including the P6/P7/P8A checklists. Open: P8A recipe robustness. Open in parallel: the interactive checklists in [`testing/phase3-assets-world.md`](testing/phase3-assets-world.md) and [`testing/phase4-sandbox.md`](testing/phase4-sandbox.md) and the clang-cl/ASan presets.

## Local verification

Run from an x64 Visual Studio Developer PowerShell with the Vulkan SDK and `VCPKG_ROOT` configured:

```powershell
cmake --fresh --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
& ".\out\build\windows-msvc\apps\umbra\Debug\Umbra.exe"
```

Additional clang-cl and AddressSanitizer verification remains available through `CMakePresets.json`. Validate scenes without the GPU:

```powershell
& ".\out\build\windows-msvc\tools\scenecheck\Debug\UmbraSceneCheck.exe" assets\scenes\settlement.scene.json --assets assets
```
