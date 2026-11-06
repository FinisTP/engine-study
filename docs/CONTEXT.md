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

### Required system interpretation

“One” defines the minimum reference content, not the universal capacity of the engine. Whether “one enemy” means one placed instance or one reusable archetype is not explicitly settled. A reversible planning default is one enemy definition reusable in separate test/reference encounters; do not expand this into multiple enemy families without need.

The reference game must demonstrate both modes, but the engine must not force every game to link/instantiate both controllers. Sharing definitions and authoritative state is encouraged; coupling turn scheduling to real-time scheduling is not.

The player-facing runtime learned AI is a compact policy choosing allowed actions. It is not a requirement for an LLM to generate executable combat logic, learn on the player's machine, or converse freely with every NPC.

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

Phase 3 (2026-11-01) added the general world, glTF hierarchy import, generational resource handles, derived asset cache and transactional reload, all still pending Windows/GPU verification. Phase 4 (2026-11-06) added the Jolt physics adapter, a third-person player, procedural walk animation, WAV audio, interactables, a pause menu, portals between two areas and development checkpoints, also pending Windows/GPU/interactive verification. The source does not establish PBR/HDR/shadows/IBL, a skeletal animation system, RPG systems, editor, C# host, or AI integration. Do not infer completion from roadmap language or empty directories.

### Observed target graph (2026-11-06, after Phase 4)

```text
Umbra (apps/umbra: Main, ModelLibrary, SceneRuntime)
  ├─ Umbra::Gameplay → World, Physics                (player, physics bridge, interaction, areas, pause menu)
  ├─ Umbra::Physics → Core; Jolt private             (bodies, raycasts, character)
  ├─ Umbra::Audio   → Core, Assets                   (WAV loader, mixer, sound bank)
  ├─ Umbra::Scene   → World, Assets, nlohmann-json   (scene files, model instancing, checkpoints)
  ├─ Umbra::Assets  → Core, tinygltf                 (glTF import, .umodel cache, file watcher)
  ├─ Umbra::World   → Core                           (entities, hierarchy, renderables)
  ├─ Umbra::Render  → Core, Diagnostics, Platform, SDL3, Volk, VMA, glm, imgui; HLSL via DXC
  ├─ Umbra::Platform → SDL3, Audio                  (window, events, input, audio device)
  ├─ Umbra::Diagnostics → spdlog
  └─ Umbra::Core    → glm                            (SlotMap, DrawList, MeshData, RetireQueue, Diagnostic, clocks)
UmbraSceneCheck → Scene, Assets                      (headless validator CLI)
Tests: Umbra.Core, .Platform, .Diagnostics.*, .Render, .World, .Assets, .Scene, .Physics,
       .Gameplay, .Audio, .SceneCheck.Sample/.Settlement/.Dungeon/.Broken, .HeadlessDependencies
```

Core, World, Assets, Scene, Physics, Gameplay, Audio and UmbraSceneCheck must stay headless; `Umbra.HeadlessDependencies` enforces it. App path: `/apps/umbra`, executable `Umbra`. Still-empty folders: `/apps/editor`, `/apps/game`, `/engine/script`, `/engine/editor`, `/samples`. Do not rename the existing app to `/apps/game` because an illustrative tree used that name.

Commit messages follow `issue #UMBRA-<n> <summary>`. Phase 3 is `#UMBRA-13` … `#UMBRA-20`; Phase 4 is `#UMBRA-21` … `#UMBRA-25`.

**Current milestone:** Phase 4, Playable 3D sandbox, is implemented and verified headlessly on Linux. The Windows build, GPU, audio and interactive checks are open (`docs/testing/phase4-sandbox.md`), as are Phase 3's (`docs/testing/phase3-assets-world.md`). Next: W0, then P5.

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

Also outside initial scope: arbitrary mid-attack mode conversion; train-on-player-device/federated learning; training large foundation models from scratch; always-on runtime conversational LLMs; full visual scripting; seamless open-world streaming; networking replication; ray tracing; custom production physics; universal asset-format support; advanced ECS/jobs before measurements justify them.

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
| Art style; skeletal animation (camera = third-person orbit, collision = Jolt, animation = procedural only: decided 2026-11-06) | When reference presentation needs it |
| One enemy instance vs reusable archetype; encounter arrangement | P5/P6 reference content |
| Turn order/AP rules, real-time attack/cost timing, effect clocks and mode-transition policy | P5 |
| Skill-tree respec, inventory overflow, quest failure, save-point UX | P5/P6 |
| Exact source schema/descriptor implementation, serialization library | Relevant P3/P5 data slice |
| Training hardware, dependency stack, algorithm, reward weights, policy thresholds and difficulty profile | P8A experiment |
| Local authoring model, bridge, download size/license; optional cloud credential/cost UX | P8B |
| Reference PC, resolution, workload, frame/resource/iteration budgets | Performance gate before P9 |
| Browser/backend/shader path and browser support set | Early W0 experiment; finalize P10 |
| First mobile OS/device tier | P11 |
| Community collection/retraining/update design and service operations | Separately approved post-release track |

The roadmap contains an open decision register with reversible defaults and revisit triggers. The implementing agent should resolve only the choices required for its current slice and document the result.

## 10. Maintaining this context

Change confirmed requirements only after a user decision; record which prior decision it supersedes. Update documentary status only with dated evidence, preserving the distinction between inherited reports and new verification. Put detailed command logs and per-task progress in the roadmap/evidence system, not this stable context brief.

Keep these three files in sync. Do not repeatedly append session transcripts or duplicate the entire roadmap here. A small corrected context is more useful than an unbounded memory dump.
