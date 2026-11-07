# ADR 0001: Web backend path (roadmap W0, decision O13)

- Status: **accepted** (2026-11-07); browser run done in Microsoft Edge, Emscripten and automatic shader conversion still untested
- Context: W0 asks, before more platform code accumulates, whether Umbra can reach the browser
  (C01: playable web export after PC) and what that means for the architecture.
- Sample: [`samples/web`](../../samples/web/README.md)

## What was tried and what happened

| Question | Tried | Result |
|---|---|---|
| Does the simulation build to WebAssembly? | The unmodified `Umbra::Rpg` and `Umbra::TurnCombat` sources plus a 90-line export file, wasi-sdk 25 (clang 19), `wasm32-wasip1`, `-O2`, no exceptions | **Yes.** 217 KB module, compiles without source changes or warnings. Imports only six WASI functions, all stubbable. |
| Do the rules give the same results? | Reference turn fight (hero vs wolf + bandit, conventional AI) in Node 22 | **Yes, exactly:** 26 actions, round 12, hero at 32 health, same as Linux GCC/Clang and Windows MSVC (2026-11-07). The rules use integers only and no randomness. |
| Can the scene be drawn in a browser? | WebGPU from JavaScript, WGSL port of `Scene.hlsl`, the 19 cubes of `settlement.scene.json`, orbit camera, depth buffer | **Yes, in Microsoft Edge on Windows 11** (user's PC, Intel Xe "gen-12lp" adapter, page opened from `file://`): the WGSL compiled without messages and the settlement rendered with depth and the same lighting. First run failed with a bug of the sample (a 74-byte index upload; `writeBuffer` needs a multiple of 4), fixed. |
| Persistent storage? | `localStorage` write/read with try/catch, counted across reloads | **Yes, in Edge:** the counter read back 1, 2, 3 over three loads of the page. |
| Same rules in the browser? | The WebAssembly module in Edge | **Yes:** 26 actions, round 12, hero at 32 health, identical to native. |
| Emscripten + Emdawnwebgpu (roadmap's candidate) | `emsdk install latest` | **Not tested:** this environment blocks the toolchain download (HTTP 403 from storage.googleapis.com). |
| Automatic HLSL → WGSL | naga / Tint | **Not tested:** package registries were unreachable from this session. |

## Decision

1. **Simulation:** keep all rules, gameplay and content code free of platform APIs, threads and
   exceptions as now; it already compiles to WebAssembly unchanged. The headless dependency tests
   (`Umbra.HeadlessDependencies`, `*OnlyDependencies`) are what keep it that way.
2. **Graphics:** WebGPU is the browser path (WebGL 2 would mean a second, older shading model).
   The Vulkan renderer stays as it is; the web build gets a separate, small WebGPU backend behind the
   same `core::DrawList` interface rather than a general RHI layer.
3. **Toolchain:** Emscripten (with Emdawnwebgpu for C++ WebGPU calls) stays the candidate for the
   full web build in P10, because the renderer, SDL3 and audio need more than WASI offers. wasi-sdk is
   enough for the rules-only module and is what the sample uses.
4. **Shaders:** HLSL stays the single source. For the web, the plan is HLSL → SPIR-V (DXC, as
   today) → WGSL (Tint or naga) at build time, with the WGSL validated in the build. Until that runs,
   `samples/web/scene.wgsl` is a hand port.

## Findings that affect the architecture now

- **No push constants in WebGPU.** `Scene.hlsl` passes the model matrix and color as push constants;
  WebGPU has none. The web version uses one uniform buffer with a 256-byte slot per draw and a dynamic
  offset. Keep per-draw data small and fixed-size so both backends can carry it.
- **No combined image samplers.** WGSL binds textures and samplers separately; `Scene.hlsl` already
  declares them separately, which converts cleanly, but the `[[vk::combinedImageSampler]]` attribute
  must not leak into new shaders that the web needs.
- **No Y flip.** The Vulkan renderer flips Y in the projection; WebGPU's clip space has +Y up and the
  same 0..1 depth, so the web backend uses `perspectiveRH_ZO` without the flip.
- **Exceptions off** for the wasm build (`-fno-exceptions`): engine code must keep reporting errors with
  return values and diagnostics, never throwing.
- **No files in the browser.** Content reaches the module as text in memory (`ParseContent`, not
  `LoadContent`); asset loading needs a fetch-based path in P10.
- **Threads:** none were needed. The real-time controller, Jolt and audio will need a decision on
  SharedArrayBuffer (requires cross-origin isolation headers: COOP/COEP) or single-threaded builds.
- **Memory:** the rules module runs in 128 KiB of linear memory for the reference fight.

## Limitations and revisit triggers

- Tested only in Edge (Chromium) on Windows with an Intel GPU. Run `samples/web/umbra-web.html` in
  Chrome, Firefox and Safari before P10 and record the results here. **Revisit** if WebGPU rendering
  fails on a target browser (WebGL 2 would then be the fallback to evaluate).
- WebGPU's `writeBuffer` needs sizes that are multiples of 4 bytes: 16-bit index buffers with an odd
  count need padding of the *buffer*, not of the data written.
- Install emsdk on a machine with access to its downloads and build the sample with `emcc`; **revisit**
  the toolchain decision if Emscripten cannot build `Umbra::Rpg` unchanged.
- Convert `Scene.hlsl` with DXC + Tint/naga; **revisit** the shader decision if the generated WGSL
  needs hand edits beyond the push-constant change.
- Port risk kept open in the roadmap (R04) until the browser run and the automatic shader path pass.
