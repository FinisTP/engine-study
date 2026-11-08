# Third-party notices (Umbra game package)

The game package (`scripts\package.bat`) ships the program, its libraries, the assets and this
file. The full license text of every library is in `licenses/` (copied from vcpkg's `copyright`
files at packaging time). This list is checked by `UmbraTool package-check`: every DLL in the
package must be named here.

## Libraries in the game

| Library | Used for | License | Files shipped |
|---|---|---|---|
| SDL3 | window, input, audio device | zlib | `SDL3.dll` |
| spdlog | logging | MIT | `spdlog.dll` |
| {fmt} | text formatting (spdlog) | MIT | `fmt.dll` |
| Jolt Physics | physics | MIT | `Jolt.dll` (if built as a DLL) |
| Dear ImGui | diagnostics overlay | MIT | inside `Umbra.exe` |
| GLM | math | MIT (or Happy Bunny) | inside `Umbra.exe` |
| nlohmann/json | content and save files | MIT | inside `Umbra.exe` |
| TinyGLTF (with stb_image) | model import | MIT (stb: MIT or public domain) | inside `Umbra.exe` |
| volk | Vulkan function loading | MIT | inside `Umbra.exe` |
| Vulkan Memory Allocator | GPU memory | MIT | inside `Umbra.exe` |
| Microsoft Visual C++ runtime | C++ standard library | Microsoft Visual Studio redistributable terms | `msvcp140*.dll`, `vcruntime140*.dll`, `concrt140.dll`, `vccorlib140.dll`, `vcomp140.dll` |

The Vulkan loader (`vulkan-1.dll`) comes with the graphics driver and is not shipped.

The browser version (`apps/umbra/web`) contains the same libraries (minus the Microsoft runtime,
volk and Vulkan Memory Allocator) compiled into `umbra.wasm`, plus the Emscripten runtime and its
libc/libc++ (MIT / University of Illinois NCSA; LLVM libc++: Apache-2.0 with LLVM exception).

Not in the game package (development only): the editor and its ImGuizmo, cpp-httplib (local AI
bridge), Catch2 (tests), Python/PyTorch (policy training), Ollama, ComfyUI and their models.

## Assets

| Assets | Origin | Terms |
|---|---|---|
| `models/hero.glb`, `elder.glb`, `bandit.glb`, `wolf.glb` | Generated with Meshy (`tools/meshy/README.md`) | Paid Meshy plan: commercial use allowed; free plan: CC BY 4.0 (credit Meshy). Check which plan made them before selling |
| `models/tower.gltf`, `tower.bin` | Hand-made sample for this project (Phase 3) | Project's own |
| `audio/*.wav` | Synthesized by `tools/audio/make_sounds.py` (sine waves and noise) | Project's own |
| `textures/sample_bricks.png` | Drawn by `tools/textures/make_sample_bricks.py` | Project's own |
| `policies/*.policy.json` | Trained for this project (`tools/policy`) | Project's own |
| AI-made materials and models | Local SDXL / TRELLIS.2; each records its model and license in its `.material.json` / `.source.json` | As recorded there; `UmbraTool package-check` warns about any non-commercial one |
