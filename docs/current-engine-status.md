# Umbra Engine - Current Development Status

**Status date:** 2026-11-07 (Phase 1/2 records dated 2026-09-02)
**Current milestone:** Phase 7 - Creator workflow, implemented; editor and package not yet run on Windows
**Phase 1 status:** Complete (interactive checklist unsigned)
**Phase 2 status:** Complete
**Phase 3 status:** Implemented. Headless tests verified on Linux; Windows build and GPU checks not run yet
**Phase 4 status:** Implemented. Headless tests verified on Linux (18/18, GCC/Clang/ASan); Windows, GPU, audio and interactive checks not run yet
**Phase 5 status:** Implemented. Headless tests verified on Linux (24/24, GCC/Clang/ASan); Windows build/CTest not run yet; wired into the game in P6
**Phase 6 status:** Implemented. Headless tests verified on Linux (27/27, GCC/Clang/ASan); Windows build 27/27 on 2026-11-07; first play test done (fixes in `#UMBRA-43`: save stone reach, separate battle arena, hit flash); full play-through open
**Phase 7 status:** Implemented. Headless tests verified on Linux (32/32, GCC/Clang/ASan); the editor compiles but has not been run; Windows checklist open

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
| Content | `Umbra::Rpg`: versioned `umbra.rpg` JSON (statuses, skills, items, actor archetypes), validation, `assets/rpg/reference.rpg.json` | `engine/rpg/include/Rpg.md` |
| State and rules | Characters (ids/counts), derived stats, inventory/equipment transactions, skill tree, shared action resolver | `engine/rpg/include/Rpg.md` |
| Turn combat | `Umbra::TurnCombat`: speed rounds, conventional enemy, `TurnEnvironment` (reset/legal actions/step) | `engine/turncombat/include/TurnCombat.md` |
| Real-time combat | `Umbra::RealtimeCombat`: wind-up/commit/active/recovery, cone hits, cooldowns, conventional enemy | `engine/realtimecombat/include/RealtimeCombat.md` |
| Integration | `Umbra::Encounter`: one fight at a time, stale-decision rejection, shared progression | `engine/encounter/include/Encounter.md` |

## Update 2026-11-06

- Phase 4 (P4.1–P4.5) implemented in commits `#UMBRA-21` … `#UMBRA-25`. Evidence and checklists: [`testing/phase4-sandbox.md`](testing/phase4-sandbox.md).
- User decisions: third-person orbit camera; Jolt behind `Umbra::Physics`; procedural animation only; W0 after P4.

## Phase 4 completed work (implemented 2026-11-06)

| Area | What exists | Explained in |
|---|---|---|
| Physics | `Umbra::Physics`: Jolt 5.6 adapter (bodies, raycasts, `CharacterVirtual`), fixed 60 Hz steps, collider scene field, debug outlines (F1) | `engine/physics/include/Physics.md`, `engine/render/src/DebugDraw.md` |
| Player | `gameplay::PlayerController`: camera-relative movement, sprint, jump, wall-aware orbit camera, interaction ray; mouse capture | `engine/gameplay/include/PlayerController.md` |
| Presentation | Procedural bob/lean (`StrideAnimator`), `Umbra::Audio` (WAV, mixer, sound bank) + SDL audio device, interactables, HUD, Esc pause menu, missing-asset warnings | `engine/audio/include/Audio.md`, `engine/gameplay/include/StrideAnimator.md` |
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

## Current architecture

```text
Umbra executable (apps/umbra: Main, ModelLibrary, SceneRuntime)
|- Umbra::Encounter  coordinator for both combat modes   -> TurnCombat, RealtimeCombat   (not linked by the app yet)
|- Umbra::TurnCombat turn controller, TurnEnvironment    -> Rpg
|- Umbra::RealtimeCombat real-time controller           -> Rpg
|- Umbra::Rpg        content, characters, inventory, skills, shared resolver -> Core, nlohmann-json
|- Umbra::Gameplay   player, physics bridge, interaction, areas, pause menu -> World, Physics
|- Umbra::Physics    Jolt adapter (private dependency)  -> Core, Jolt
|- Umbra::Audio      WAV loader, mixer, sound bank      -> Core, Assets
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

Next: the Windows checklists for P6/P7, then P8A / P8B. Open in parallel: the interactive checklists in [`testing/phase3-assets-world.md`](testing/phase3-assets-world.md) and [`testing/phase4-sandbox.md`](testing/phase4-sandbox.md) and the clang-cl/ASan presets.

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
