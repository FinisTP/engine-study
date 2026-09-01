# Umbra Engine — Current Development Status

**Status date:** 2026-09-01  
**Current milestone:** Phase 1 — Foundation  
**Phase 1 status:** Automated exit checks complete; final interactive sign-off pending

## Project summary

Umbra is a learning-first, modular 3D game engine written in C++20. The first product target is Windows x64, followed by Android ARM64 after the desktop runtime and renderer are stable.

Phase 1 establishes the application lifetime, deterministic timing loop, input and lifecycle contracts, diagnostics, tests, compiler policies, sanitizers, and CI needed before Vulkan work begins.

## Verified build status

Fresh configure, build, and CTest runs completed successfully on 2026-09-01:

| Configuration | Build | CTest |
|---|---:|---:|
| MSVC x64 Debug | Passed | 7/7 passed |
| clang-cl x64 Debug | Passed | 7/7 passed |
| MSVC x64 Debug + AddressSanitizer | Passed | 7/7 passed |

The registered CTest coverage is:

- `Umbra.Core`
- `Umbra.Platform`
- `Umbra.Diagnostics.Formatting`
- `Umbra.Diagnostics.fatal`
- `Umbra.Diagnostics.assert`
- `Umbra.Diagnostics.terminate`
- `Umbra.Diagnostics.windows-exception`

The interactive MSVC Debug application was launched and exited normally with code 0. The complete visual/lifecycle checklist in [`testing/phase1-smoke-test.md`](testing/phase1-smoke-test.md) still requires recorded human sign-off because focus, minimize/restore behavior, and resize feedback cannot be verified from the process exit code alone.

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
| Graphics direction | Vulkan through Volk and VMA |

GLM, Volk, Vulkan Headers, and Vulkan Memory Allocator are installed but are not yet used by a runtime renderer.

## Current architecture

```text
Umbra executable
├─ Umbra::Core
├─ Umbra::Diagnostics
└─ Umbra::Platform
   ├─ Umbra::Diagnostics
   └─ SDL3

UmbraCoreTests
├─ Umbra::Core
└─ Catch2

UmbraPlatformTests
├─ Umbra::Platform
└─ Catch2

UmbraDiagnosticsTests / UmbraDiagnosticsProbe
└─ Umbra::Diagnostics
```

Shared interface targets provide centralized warning and AddressSanitizer options without leaking third-party implementation details through public engine headers.

## Phase 1 completed work

### Build and verification

- Modular CMake target graph and C++20 configuration
- MSVC, clang-cl, and MSVC AddressSanitizer configure/build/test presets
- Centralized MSVC and clang-cl warning policies
- Local `scripts/verify-phase1.ps1` verification entry point
- Windows GitHub Actions matrix for MSVC, clang-cl, and AddressSanitizer
- Clean MSVC and clang-cl builds under the configured warning policy

### Core timing

- Monotonic `FrameClock` with injectable time source
- Frame-delta clamping, reset, repeated-sample, and backward-time behavior
- Fixed-step accumulation integrated into the application loop
- Deterministic accumulator tests for empty state, ignored invalid deltas, exact and multiple steps, interpolation remainder, and reset

### Platform and application lifecycle

- SDL video lifetime behind `Platform`
- RAII fallback cleanup with deleted copy and move operations
- Single initialization state and idempotent `noexcept Shutdown()`
- Vulkan-capable, resizable RAII window
- Programmatic resize kept separate from event-driven cached-size updates
- Input pressed, held, released, repeat, unknown-key, and clearing behavior
- Frame-local quit, close, resize, focus, suspend, resume, and termination events
- Suspended frames skip simulation; resume resets clock and accumulator state

### Diagnostics

- Engine-owned info, error, critical, flush, assertion, and fatal APIs
- spdlog remains a private Diagnostics dependency
- C++20 source-location reporting and deterministic failure formatting
- Debug assertion macro with a Release-disabled form
- Predictable fatal/assertion logging, flushing, debugger break, and abort behavior
- Idempotent `std::terminate` and Windows unhandled-exception handler installation
- Recursive-crash protection and previous-handler restoration
- Child-process tests for all terminating paths, with OS crash-dialog suppression and timeouts

## Remaining Phase 1 sign-off

Only external/manual confirmation remains before Phase 1 can be declared closed:

1. Let the new GitHub Actions workflow complete on its first pushed commit or pull request.
2. Complete and record every item in [`testing/phase1-smoke-test.md`](testing/phase1-smoke-test.md), including resize, focus loss, minimize/resume, Escape, native close, and shutdown-log ordering.
3. After those two checks pass, update the main roadmap to mark Checkpoint 1 complete and begin Phase 2.

## Phase 2 entry point

Phase 2 begins with:

1. Vulkan loader and validation-layer initialization through Volk.
2. Vulkan instance and SDL surface creation.
3. Physical-device selection and queue-family discovery.
4. Logical-device and swapchain ownership.
5. Command submission, synchronization, and resize-safe swapchain recreation.
6. A captured validation-clean triangle before higher-level renderer abstractions.

## Local verification

Run from an x64 Visual Studio Developer PowerShell with `VCPKG_ROOT` set:

```powershell
.\scripts\verify-phase1.ps1
```

Individual configurations remain available through `CMakePresets.json`.
