# Umbra Engine - Current Development Status

**Status date:** 2026-11-01 (Phase 1/2 records dated 2026-09-02)
**Current milestone:** Phase 3 - Assets and world, implemented; Windows/GPU gate open
**Phase 1 status:** Complete (interactive checklist unsigned)
**Phase 2 status:** Complete
**Phase 3 status:** Implemented. Headless tests verified on Linux; Windows build and GPU checks not run yet

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
|- Umbra::Scene      scene files, model instancing      -> World, Assets, nlohmann-json
|- Umbra::Assets     glTF import, .umodel cache, watcher -> Core, tinygltf
|- Umbra::World      entities, hierarchy, renderables    -> Core
|- Umbra::Render     Vulkan renderer, draws a DrawList   -> Core, Diagnostics, Platform, SDL3, Volk, VMA, ImGui
|  |- SceneResources: generational mesh/texture handles, fallback texture, retire queue
|  |- Indexed HLSL scene pipeline: set 0 camera, set 1 texture, push constants per draw
|  `- Swapchain, depth, two frames in flight, ImGui diagnostics overlay
|- Umbra::Platform   SDL3 window, events, input
|- Umbra::Diagnostics logging, crash handling            -> spdlog
`- Umbra::Core       SlotMap, DrawList, MeshData, RetireQueue, Diagnostic, clocks -> glm
UmbraSceneCheck      headless scene/model validator      -> Scene, Assets
```

Headless rule (checked by `Umbra.HeadlessDependencies`): Core, World, Assets, Scene and UmbraSceneCheck never link Render, Platform, SDL3, Vulkan or ImGui.

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
- Gameplay-state preservation across scene reloads (checkpoints are P4.4).
- Packaging: the app reads assets from the source tree (`UMBRA_ASSET_DIRECTORY`) and caches in the build tree. A relocatable package layout is a P6/P9 task (roadmap T43).

Next: run the Windows/GPU gate in [`testing/phase3-assets-world.md`](testing/phase3-assets-world.md), then W0 (web feasibility) and P4 (playable sandbox).

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
& ".\out\build\windows-msvc\tools\scenecheck\Debug\UmbraSceneCheck.exe" assets\scenes\sample.scene.json --assets assets
```
