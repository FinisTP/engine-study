# Umbra in the browser (roadmap P10)

The same game, compiled to WebAssembly with Emscripten, drawn with WebGPU. Supported browsers:
**Chrome and Edge** (WebGPU, version 113 or newer) on a computer with a recent graphics driver
(user decision 2026-11-08; Firefox and Safari may work but are not tested). The build is
**single-threaded** so any static web host works (no special headers).

## Play it on your PC

1. Put a web build in `out\web` (see "Build" below; or copy the folder you were given).
2. Double-click `scripts\serve-web.bat`. It first packs your current `assets` folder into the
   page (`UmbraTool web-pack`, from your editor build), then serves the folder on
   `http://localhost:8080/` and opens the browser. So scenes and content you saved in the editor
   show up without a new web build; only C++ changes need one. (Opening `index.html` as a file does not work: browsers only load WebAssembly and
   install offline pages from a web address.)
3. **Click to play.** Keys as on the PC; F1 shows the timing box.

## What the page does

| Part | How |
|---|---|
| Game | `umbra.js` + `umbra.wasm` (the C++ game) |
| Assets | `assets.bin` (the `assets` folder's files back to back, AI working files left out) + `assets.json` (where each file sits). Written by `UmbraTool web-pack <assets> <page folder>`; the page copies them into the game's file system before `main` and refuses a pair that doesn't match |
| Picture | `engine/render/web/RendererWeb.cpp` + `umbra_render.js`: the `Renderer` interface drawn with WebGPU (`WebRenderer.md`) |
| HUD and menus | HTML over the canvas, filled from the same `core::Hud` the PC version draws with ImGui |
| Saves | Emscripten's IDBFS: the saves folder lives in the browser's IndexedDB; written after every save. **Export saves** downloads them as `umbra-saves.json`, **Import saves** loads such a file (browsers may clear site data, so export what matters) |
| Starting again | the same start menu as the PC: Continue / Load the other save / New game |
| Offline | `sw.js` (service worker) caches one complete version (`cache.addAll` is all-or-nothing). The corner says **Ready offline (version)** only when every file of that version is cached. The version is `program+assets` (two hashes), so repacked assets are a new version too. Files come from the server first and from the cache only when the server can't be reached |
| No WebGPU | a clear message; the game is not downloaded |
| Graphics device lost | a message to reload; the last save is kept |
| Tab hidden | the game stops with the browser's animation frames and continues where it was |

## Build

The game builds with any recent Emscripten (3.1.52 was used, in the cloud workspace with the
system LLVM 18 and binaryen built from source).

```text
# once: emsdk (https://emscripten.org/docs/getting_started/downloads.html), then build the
# libraries for WebAssembly into a folder, e.g. with emcmake + cmake --install:
#   SDL3, spdlog, JoltPhysics/Build (CPP_RTTI_ENABLED=OFF), glm, nlohmann-json, Catch2 (tests),
#   and copy tiny_gltf.h, json.hpp, stb_image*.h into <prefix>/include
WEB_PREFIX=<prefix> tools/web/build-web.sh        # Linux/macOS shell; builds and runs the Node tests
```

The page lands in `<build>/apps/umbra`: `index.html`, `umbra-page.js/.css`, `umbra.js`,
`umbra.wasm`, `sw.js` (its version is a hash of those files, `stamp.cmake`), and `assets.bin` /
`assets.json` from `UmbraTool web-pack` (`build-web.sh` runs it with a native UmbraTool; set
`UMBRA_TOOL` if it isn't in a `build*` folder).
Copy those to any static host. `tools/web/check-web.py` runs the browser checks (Playwright).

## Limits

- The WGSL shader is a hand port of `Scene.hlsl` (`umbra_render.js`); automatic HLSL → WGSL is
  still open (ADR 0001).
- One thread: physics, AI and the learned enemy run on the page's thread (the enemy's network
  takes microseconds, so it decides at once instead of on a worker).
- Audio uses SDL's ScriptProcessor path (Chrome warns it is deprecated); sound starts after the
  first click, as browsers require.
- No editor, no AI tools in the browser (C02).
