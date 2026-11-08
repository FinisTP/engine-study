# Browser renderer (P10)

`RendererWeb.cpp` implements the same `Renderer` class as the Vulkan renderer, so the game code
does not change. The GPU work is in `umbra_render.js` (an Emscripten JavaScript library):

1. **Initialize** asks the browser for a WebGPU adapter and device (asynchronous: Emscripten's
   Asyncify lets the C++ call wait for the JavaScript promise). No WebGPU → `false`, logged.
2. **CreateMesh / CreateTexture** hand vertex, index and pixel arrays straight from WebAssembly
   memory to the GPU; JavaScript returns a small id, kept in a `SlotMap` like the Vulkan handles.
   Textures upload every mip level (`core::MipChain`).
3. **RenderFrame** packs one record per draw (model matrix, color × mesh color, texturing, mesh id,
   texture id: 28 floats; emissive in float 21), the frame block (3 matrices and
   `core::LightingBlock`: 208 floats), the debug lines and the HUD as JSON, and calls JavaScript once.
   There, each draw's data goes into a 256-byte slot of one uniform buffer, bound with a dynamic
   offset: WebGPU has no push constants (ADR 0001). Two passes follow:
   - the **shadow pass** draws depth from the sun into a 2048² `depth32float` texture (skipped
     indoors). It uses its own bind group without that texture, because WebGPU forbids reading a
     texture in the pass that draws into it;
   - the **scene pass** draws the sky triangle and the lit objects (sun, shadow compare, 16 lights,
     fog) into an `rgba16float` HDR picture. The math is in `engine/core/include/Lighting.md`;
   - the **post passes** (S7, `engine/core/include/Post.md`) build bloom (6 levels starting at half
     size, as on the PC). The composite onto the canvas then applies exposure, tonemapping, the
     16³ color table (`umbra_gpu_lut`, uploaded only when the grade changes) and the vignette.
     The debug lines are drawn after it, and the game UI after that (`umbra_gpu_ui`).

   Draw float 26 picks a custom surface shader's pipeline (`umbra_gpu_surface`). Its WGSL was made
   at build time from `Scene.hlsl` by DXC and naga (`engine/render/shaders/CustomShaders.md`).
4. Then it **waits for the browser's next animation frame**. This is where the game's normal
   `while` loop hands control back to the page (Asyncify), so the loop needs no browser-specific
   structure and runs at the display's rate.

Differences from Vulkan: no Y flip in the projection (WebGPU clip space has +Y up), so the shadow
lookup uses `uv.y = 0.5 - 0.5*y`; no ImGui (the HUD is HTML); textures and samplers bound separately;
`writeBuffer` sizes rounded up to 4 bytes.

Colors: textures are `rgba8unorm-srgb` and the canvas (usually `bgra8unorm`) is configured with an
`-srgb` view format, so drawing happens in linear light and the GPU converts on write, as on Vulkan.

The scene and post WGSL in `umbra_render.js` are hand copies of `Scene.hlsl` and `Post.hlsl`:
change both together. (Custom surface shaders are converted automatically.) To check it,
extract the string and run `naga scene.wgsl` (naga-cli validates WGSL).

Testing options: `index.html?args=--hour%2012` passes command-line options to the game.
