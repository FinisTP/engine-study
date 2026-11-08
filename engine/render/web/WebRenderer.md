# Browser renderer (P10)

`RendererWeb.cpp` implements the same `Renderer` class as the Vulkan renderer, so the game code
does not change. The GPU work is in `umbra_render.js` (an Emscripten JavaScript library):

1. **Initialize** asks the browser for a WebGPU adapter and device (asynchronous: Emscripten's
   Asyncify lets the C++ call wait for the JavaScript promise). No WebGPU → `false`, logged.
2. **CreateMesh / CreateTexture** hand vertex, index and pixel arrays straight from WebAssembly
   memory to the GPU; JavaScript returns a small id, kept in a `SlotMap` like the Vulkan handles.
   Textures upload every mip level (`core::MipChain`).
3. **RenderFrame** packs one record per draw (model matrix, color × mesh color, texturing, mesh id,
   texture id: 28 floats), the debug lines and the HUD as JSON, and calls JavaScript once. There,
   each draw's data goes into a 256-byte slot of one uniform buffer, bound with a dynamic offset:
   WebGPU has no push constants (ADR 0001). One render pass draws the objects, then the lines.
4. Then it **waits for the browser's next animation frame**. This is where the game's normal
   `while` loop hands control back to the page (Asyncify), so the loop needs no browser-specific
   structure and runs at the display's rate.

Differences from Vulkan: no Y flip in the projection (WebGPU clip space has +Y up), no ImGui
(the HUD is HTML), textures and samplers bound separately, `writeBuffer` sizes rounded up to
4 bytes.
