# Umbra Engine - Current Development Status

**Status date:** 2026-09-02
**Current milestone:** Phase 3 - Assets and world
**Phase 1 status:** Complete
**Phase 2 status:** Complete

## Update 2026-11-01

- Plan and requirements moved to [`ROADMAP.md`](ROADMAP.md) and [`CONTEXT.md`](CONTEXT.md); agent rules in [`../CLAUDE.md`](../CLAUDE.md).
- B0 baseline reconciliation is in progress: [`testing/b0-baseline-reconciliation.md`](testing/b0-baseline-reconciliation.md). Core/Diagnostics/Platform tests pass on Linux/GCC; Windows presets not re-run.
- Phase 3 has started: uncommitted `engine/world` stub (`Umbra::World`, empty `World` class).
- Phase 1 interactive checklist (`testing/phase1-smoke-test.md`) is still unsigned.

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
| Scene interchange | glTF 2.0 through TinyGLTF |
| Diagnostics UI | Dear ImGui with an Umbra-owned Vulkan renderer |

## Current architecture

```text
Umbra executable
|- Umbra::Core
|- Umbra::Diagnostics
|- Umbra::Platform
|  `- SDL3
`- Umbra::Render
   |- Vulkan instance, surface, physical-device selection, and logical device
   |- VMA allocator and staging uploads
   |- SceneResources with mesh and texture handles
   |- Swapchain and per-image color views
   |- Indexed HLSL scene pipeline and per-image depth attachments
   |- Two frame slots with commands, synchronization, uniforms, and descriptors
   `- ImGui diagnostics overlay
```

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

## Phase 3 entry point

Phase 3 begins with the Assets and world milestone:

1. Define world/entity and transform ownership without exposing graphics API handles.
2. Replace the single hard-coded scene primitive with a renderable draw list.
3. Introduce generational resource handles and explicit loading states.
4. Separate source glTF import from a compact, versioned runtime representation.
5. Traverse glTF nodes and preserve hierarchy, transforms, meshes, and materials.
6. Add dependency tracking and safe asset reload behavior.
7. Complete the checkpoint by editing a source asset and observing a safe derived-asset reload.

## Local verification

Run from an x64 Visual Studio Developer PowerShell with the Vulkan SDK and `VCPKG_ROOT` configured:

```powershell
cmake --fresh --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
& ".\out\build\windows-msvc\apps\umbra\Debug\Umbra.exe"
```

Additional clang-cl and AddressSanitizer verification remains available through `CMakePresets.json`.
