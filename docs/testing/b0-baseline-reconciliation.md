# B0 baseline reconciliation

## Test record

- Date: 2026-11-01
- Tester: Claude (remote session), source inspection + cloud Linux build
- Repository: `main` at `63e6fe7` (2026-09-02), dirty: `CMakeLists.txt` and `apps/umbra/CMakeLists.txt` modified (World wiring); `engine/world/` and `Exam1.txt` untracked
- Gate / cases: B0
- State: **in progress**. Windows checks not run.

## What was checked

| Check | Result | Evidence source |
|---|---|---|
| Source, CMake, presets, vcpkg manifest and docs readable | Done | Source inspection |
| Status doc vs checkout | Mostly consistent; see discrepancies | Source inspection |
| Core/Diagnostics/Platform build (Linux GCC 13.3, Debug, `-Wall -Wextra -Wpedantic`) | Passed, 0 warnings | Run in cloud workspace |
| CTest: `Umbra.Core`, `Umbra.Diagnostics.Formatting`, `.fatal`, `.assert`, `.terminate`, `Umbra.Platform` | 6/6 passed | Run in cloud workspace |

## Environment of the Linux run

- Ubuntu 24.04 container, GCC 13.3, CMake 3.28.3, Ninja.
- Dependencies built from source (vcpkg downloads were blocked): Catch2 v3.15.3, spdlog v1.15.3, SDL 3.2.24. These are not the vcpkg-pinned builds.
- Scratch top-level CMake included `cmake/UmbraWarnings`, `cmake/UmbraSanitizers` and `add_subdirectory` of `engine/core`, `engine/diagnostics`, `engine/platform`, `tests/core`, `tests/diagnostics`, `tests/platform`. No repository file was changed.
- Run with `SDL_VIDEO_DRIVER=dummy`.

## Discrepancies found

1. The status doc says Phase 1 is complete, but `phase1-smoke-test.md` is "Partial" with every checklist item unticked.
2. Uncommitted P3.1 stub: `engine/world` (empty `umbra::world::World`, target `Umbra::World`), linked into `Umbra`. No tests yet.
3. `.github/workflows/windows.yml` sets `VCPKG_ROOT: E:\Projects\vcpkg`. Hosted `windows-latest` runners don't have this path, so CI can probably not configure. No CI run results were checked.
4. `cmake/UmbraSanitizers.cmake` defines `umbra_copy_asan_runtime` twice; `engine/core/CMakeLists.txt` repeats one `target_link_libraries` block. Harmless.
5. Older text listing only `UmbraCoreTests` is stale. Real tests cover Core, Diagnostics (plus crash probes), Platform and Render.

## Not run

- `windows-msvc`, `windows-clang`, `windows-msvc-asan` configure/build/CTest. No Windows toolchain is reachable from the session.
- `Umbra.Render`, `Umbra.Diagnostics.windows-exception`, the `Umbra` app, Vulkan validation, resize/minimize, RenderDoc.
- Phase 1 interactive checklist.

## To close B0

1. User runs on Windows: `cmake --preset windows-msvc`, `cmake --build --preset windows-msvc-debug`, `ctest --preset windows-msvc-debug` (or `scripts/verify-phase1.ps1`), then launches `Umbra.exe` once. Record results here.
2. Sign off or record failures in `phase1-smoke-test.md`.
3. Check whether GitHub Actions runs succeed; fix `VCPKG_ROOT` if they don't.
4. Decide to commit or drop the `engine/world` stub before P3.1 work builds on it.

Next task after B0: P3.1 (world identity and ownership).
