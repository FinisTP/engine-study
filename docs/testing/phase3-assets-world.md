# Phase 3 — Assets and World: verification record

## Record

- Date: 2026-11-01
- Implemented in commits `issue #UMBRA-13` (P3.1) through `issue #UMBRA-20` (P3.8), on `main`
- Tester: Claude (remote session) for automated Linux checks. **Windows and interactive checks are pending the user.**
- State: **implemented; headless parts verified on Linux; Windows/GPU gate open**

## 1. Automated checks run (Linux, cloud workspace)

Environment: Ubuntu 24.04, CMake 3.28, Ninja, Debug. Dependencies built from source at the vcpkg-pinned versions where possible: Catch2 3.15.3, spdlog 1.15.3, SDL 3.2.24, glm 1.0.3, nlohmann-json 3.12.0, tinygltf 3.0.0, volk/Vulkan-Headers 1.4.328, VMA 3.3.0, imgui 1.92.4. How to reproduce: [`linux-headless-build.md`](linux-headless-build.md).

| Configuration | Result |
|---|---|
| GCC 13.3, `-Wall -Wextra -Wpedantic` | Build OK, 0 new warnings, 13/13 CTest entries pass |
| Clang 18 | Build OK, 0 new warnings, 13/13 pass |
| GCC 13.3 + `-fsanitize=address,undefined` | Build OK, 13/13 pass, no sanitizer reports |

CTest entries: `Umbra.Core`, `Umbra.Platform`, `Umbra.Diagnostics.Formatting/.fatal/.assert/.terminate`, `Umbra.Render`, `Umbra.World`, `Umbra.Assets`, `Umbra.Scene`, `Umbra.SceneCheck.Sample`, `Umbra.SceneCheck.Broken` (expected to fail, inverted), `Umbra.HeadlessDependencies`.

**Limits of this evidence:**

- The whole tree, including `Umbra::Render` and the `Umbra` app, **compiles** on Linux. Nothing was run on a GPU.
- Shaders were compiled with **glslang** standing in for DXC (syntax, bindings and push-constant storage checked). DXC and `spirv-val` were **not run**.
- MSVC, clang-cl and MSVC-ASan were **not run**.
- The Linux SDL is 3.2.24, while vcpkg pins 3.4.14. spdlog differs too (1.15.3 vs 1.17.0).

## 2. Roadmap test cases (P3 gate)

| Case | Evidence | Status |
|---|---|---|
| T01 stale handle | `SlotMapTests`, `WorldTests` (slot reuse, cross-world, clear) | Passed (headless) |
| T02 corrupt/partial reload | `ModelCacheTests` "failed build keeps last good"; reload transaction in `ModelLibrary::Reload` | Headless part passed; live reload **not run** |
| T03 missing/circular reference | `SceneFileTests` (missing parent, cycle chain message), `UmbraSceneCheck` | Passed (headless) |
| T04 hierarchy cycle / deleted parent | `TransformTests` | Passed (headless) |
| T43 offline install without dev paths | App reads assets from the source tree | **Open**, packaging belongs to P6/P9 |
| T44 replaced resource still in use | `ResourceDataTests` (retire queue); design in `ResourceLifetime.md` | Logic passed; GPU/validation **not run** |
| T50 oversized/corrupt/traversing input | Importer OOB/unsafe-path tests, cache reader bounds, scene size and path checks | Passed (headless) |
| T51 reload changes active content | Structure change → scene reload | Partial (no gameplay content yet) |
| T53 headless leaks | `Umbra.HeadlessDependencies` (verified to fail on an injected leak) | Passed |
| T57 rename / duplicate durable id | Duplicate ids rejected; a renamed asset is reported missing | Partial: case-only renames on Windows not handled |

## 3. Windows gate (to run, then fill in)

From an x64 Visual Studio Developer PowerShell with the Vulkan SDK and `VCPKG_ROOT`:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
& ".\out\build\windows-msvc\apps\umbra\Debug\Umbra.exe"
```

`vcpkg.json` now lists `nlohmann-json` (it was already installed as a tinygltf dependency). Expect vcpkg to re-resolve once.

- [ ] Configure, build and all CTest entries pass (MSVC Debug)
- [ ] Same with `windows-clang` and `windows-msvc-asan`
- [ ] DXC + `spirv-val` accept the changed `Scene.hlsl`

## 4. Interactive checklist

Run `Umbra.exe` with Vulkan validation on.

**Scene and draw lists**

- [ ] Ring of 6 tinted cubes, a spinning cube with an orbiting smaller cube, and a 3-block tower in the distance
- [ ] Overlay shows Meshes 2, Textures 3 (fallback + one per model), Draws 11 (6 ring + spinner + moon + 3 tower blocks)
- [ ] No validation errors at start-up, while resizing, minimizing/restoring, or on exit

**Reload (keep the app running)**

- [ ] Edit `assets/scenes/sample.scene.json`: change `ring_0`'s `position`, save. It moves within about half a second; the log says "Loaded scene"
- [ ] Break the scene (delete a closing brace), save. The log shows `scene.parse` and the scene stays as it was. Fix it, save, and it reloads
- [ ] Edit `assets/models/tower.gltf`: change `materials[0]...baseColorFactor` to `[1,0.3,0.3,1]`, save. The tower turns red ("Reloaded models/tower.gltf.")
- [ ] Rename the `Top` node in `tower.gltf`, save. The log says "structure changed" and the scene reloads
- [ ] Truncate `tower.gltf` mid-file, save. The log shows the import error and the tower stays visible. Restore it
- [ ] No validation errors during any reload (old buffers are freed only after in-flight frames)

**Tools**

- [ ] `UmbraSceneCheck.exe assets\scenes\sample.scene.json --assets assets` prints 0 errors, exit 0
- [ ] `UmbraSceneCheck.exe tests\scene\data\broken.scene.json --assets assets --json` lists the cycle, the duplicate id and the unsafe path, exit 1

**Optional:** a RenderDoc capture showing one `vkCmdDrawIndexed` per object, with push constants and set 1 rebinding per texture.

Mark Phase 3 **verified complete** only after sections 3 and 4 pass. Record failures here with the log text.
