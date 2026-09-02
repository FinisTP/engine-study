# Phase 2 rendering smoke test

## Test record

- Date: 2026-09-02
- Tester: User-confirmed local verification
- Target: Windows x64 Vulkan runtime
- Result: Passed
- Capture: RenderDoc capture and inspection confirmed by the user; the capture remains a local development artifact

## Automated and build verification

- [x] HLSL scene and overlay shaders compile to SPIR-V and pass SPIR-V validation.
- [x] The configured project builds and registered tests pass.
- [x] Vulkan validation is enabled for the interactive debug run.

## Scene checkpoint

- [x] The controlled `phase2.gltf` asset loads one indexed primitive and one base-color texture.
- [x] Vertex and index data upload through VMA-owned staging and device buffers.
- [x] The scene renders through `vkCmdDrawIndexed` with the expected position, normal, and UV inputs.
- [x] Model, view, and Vulkan-correct projection matrices update through per-frame uniform buffers.
- [x] WASD camera movement, arrow-key look, and scene rotation work.
- [x] Texture sampling, the base-color factor, and basic directional lighting render correctly.
- [x] Per-swapchain-image depth attachments correctly hide farther fragments.
- [x] The ImGui overlay displays GPU, swapchain, frame-time, and loaded-resource diagnostics.

## Lifecycle checkpoint

- [x] Normal resizing recreates swapchain-dependent resources without reloading the scene.
- [x] Rapid resize remains validation-clean.
- [x] A minimized or zero-sized surface defers swapchain recreation instead of terminating the application.
- [x] Restore requests recreation and resumes rendering with a reset frame clock.
- [x] Escape and native-window close perform an orderly, validation-clean shutdown.
- [x] RenderDoc shows the indexed scene draw, scene descriptors, color/depth attachments, texture sampling, and overlay draws.

## Phase 2 scope boundary

Phase 2 intentionally loads a controlled first glTF mesh primitive, one base-color texture, and basic material parameters. Mip generation, full glTF scene traversal, metallic-roughness PBR, asynchronous streaming, generational resource management, and render-graph scheduling remain later-phase work.
