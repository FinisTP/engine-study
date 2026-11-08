# P10 — Playable web export: verification record

## Record

- Date: 2026-11-08
- Commit: `issue #UMBRA-66` (P10.1–P10.5)
- Tester: Claude (remote session), Linux cloud workspace: Emscripten 3.1.52 (system LLVM 18,
  binaryen 115 built from source), Node 22, headless Google Chrome for Testing 153 (Playwright).
- User decisions (2026-11-08): browsers **Chrome + Edge**; hosting **not decided → single-threaded**
  (any static host); toolchain **workaround in the cloud** (worked: no emsdk download needed).
- State: **implemented; verified headlessly except the picture.** The whole game runs in headless
  Chrome (assets, scenes, HUD, input, menus, saves, offline). This container's software GPU loses
  every WebGPU device within a second (a 10-line WebGPU page does the same), so nothing here proves
  the scene renders correctly. The rendering check is on the user's PC (§4).

## What was built

| Task | Where |
|---|---|
| P10.1 Graphics/platform | `engine/render/web/RendererWeb.cpp` + `umbra_render.js`: the `Renderer` interface on WebGPU (hand WGSL port of `Scene.hlsl`, dynamic-offset uniforms, mip levels, debug lines), HUD/menus as HTML from `core::Hud`. Root CMake builds only the game modules for `EMSCRIPTEN`. SDL3 window/input/audio unchanged; the game loop yields once per frame (Asyncify) |
| P10.2 Policy | the built-in C++ network compiled to WebAssembly; `PolicyDecider` decides at once without threads (same `Request`/`Take` contract, stale ids still dropped) |
| P10.3 Saves | IndexedDB under the SDL preferences folder (IDBFS), written after every save (`Platform::PersistFiles`), failures shown on the page; export/import as `umbra-saves.json`; start menu Continue (`#UMBRA-65`) |
| P10.4 Offline | `sw.js`: one complete version per build hash (`stamp.cmake`), all-or-nothing caching, "Ready offline (version)" only when complete, updates after a reload |
| P10.5 Lifecycle/package | no WebGPU → message, game not downloaded; device lost → message; tab hidden → frames stop (no catch-up burst: `FrameClock` caps a frame at 0.25 s); page files only (no Windows paths, no editor); `scripts\serve-web.bat` local server |

## 1. Automated checks

| Check | Result |
|---|---|
| Native (GCC, Clang, ASan) | 44/44 CTest each (nothing native changed behaviour) |
| WebAssembly in Node: 13 headless suites (core, rpg, turn, real-time, encounter, story, game incl. the 300-fight run T74, policy incl. NumPy parity of the shipped model T29, world, assets, scene, physics, gameplay) | **13/13 pass** |
| Headless Chrome (`tools/web/check-web.py`) | **9/9**: game starts and draws frames; HUD; keyboard (Tab → real time); pause menu as HTML; a saved file survives a reload (IndexedDB); "Ready offline"; starts with the network off; no script errors; no WebGPU → clear message, game not loaded |

Sizes: `umbra.wasm` 5.8 MB, `umbra.data` 3.3 MB, `umbra.js` 0.25 MB.

## 2. Exit test cases

| Test | Evidence | State |
|---|---|---|
| T35 suspend / GPU loss / storage eviction | hidden tab stops frames; device-lost message; storage failures shown; export/import | headless parts pass; real suspend/resume on the user's PC (§4) |
| T36 unsupported graphics | no-WebGPU message (headless check) | **covered** |
| T37 non-ASCII text and paths | content is UTF-8, IDs separate from names; Vietnamese text not yet shown in the HTML HUD | open (§4 item 6) |
| T38 floating-point divergence | turn rules integer; the learned model matches NumPy within tolerance in wasm; Node suites pass with the native expectations | **covered** for the rules and policy |
| T43 clean install without developer paths | page files only; offline start in headless Chrome | covered headlessly; §4 item 4 |
| T44 replaced GPU resource | WebGPU frees destroyed buffers after submitted work; textures/meshes replaced through ids | covered by design; no stress test |
| T45 partial offline install | all-or-nothing cache, ready only when complete | **covered** (`sw.js`), update path §4 item 5 |
| T52 queue/shutdown | single-threaded decider: no queue, no worker to join | **covered** (policy tests in wasm) |
| T56 pause/lifecycle timing | 0.25 s frame cap after a hidden tab; focus loss pauses (SDL) | §4 item 3 |

## 3. Limits, honestly

- The picture has not been seen: the user's check (§4 item 1) decides P10.1.
- Hand-ported WGSL (two shader sources to keep in step: `Scene.hlsl` and `umbra_render.js`).
- Audio through SDL's deprecated ScriptProcessor path; starts after the first click.
- Built only with Emscripten 3.1.52 so far; Windows build with a current emsdk not tried.
- Firefox/Safari not tested (not promised).

## 4. User checklist (Chrome or Edge on the user's PC)

| # | Do | Expect | Result |
|---|---|---|---|
| 1 | Open the test page (artifact link given in chat) or `scripts\serve-web.bat` with the build in `out\web`; **Click to play** | the settlement as on the PC (textures, bricks, characters), HUD text, ~60 FPS (F1) | |
| 2 | Walk, talk to Mara, a turn fight and a real-time fight, save at the stone | as on the PC; the learned bandit leader fights | |
| 3 | Switch to another tab for a minute, come back | paused menu or same spot; no jump | |
| 4 | `serve-web.bat`, wait for "Ready offline", stop the server, reload | the game still starts | |
| 5 | Reload the page later; close and reopen the browser | Continue loads the save | |
| 6 | Export saves, Import them in another browser profile | the saves come back | |
