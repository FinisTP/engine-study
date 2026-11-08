# P11 — Android (emulator first): verification record

## Record

- Date: 2026-11-08
- Commit: `issue #UMBRA-79` (P11.1–P11.4), ADR 0009
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, ASan/UBSan,
  Emscripten 3.1.52). The Android SDK, NDK and Gradle downloads are blocked there, so the
  Android build itself and the emulator run are for the user on Windows (§4).
- State: **implemented; verified headlessly.** Gradle/NDK build, emulator play, phones open.

## What was built

| Task | Where |
|---|---|
| P11.1 Android build | root `CMakeLists.txt` (`ANDROID`: engine modules, game plugins, game; ABI -> vcpkg triplet), `apps/umbra/CMakeLists.txt` (`libmain.so`), `plugins/android-export/` (Gradle app, SDL 3.4.14 Java activity, game-only vcpkg manifest), `scripts\build-android.bat` |
| P11.2 Game pack | `UmbraTool game-pack`, `authoring::PackGame`, `assets::UnpackGame`; shaders now built for Vulkan 1.1 (SPIR-V 1.3) for phones; VMA told 1.1 |
| P11.3 Lifecycle | `EnteredBackground`/`EnteredForeground` events; save + pause + `Renderer::ReleaseSurface`; `RestoreSurface`; `VK_ERROR_SURFACE_LOST_KHR` releases too; SDL told not to block the loop in the background; back button = Escape; log to logcat (tag `Umbra`) |
| P11.4 Touch and gamepad | `apps/umbra/TouchControls.cpp` (floating stick, camera drag, buttons; fight hotkeys 1-4 and Flee); touch fingers and gamepad axes in `platform::Input`; touch-made mouse events only point |

How it works: `apps/umbra/Mobile.md`. Building: `plugins/android-export/README.md`.

## 1. Automated checks

| Check | Result |
|---|---|
| Linux GCC / Clang / GCC ASan+UBSan | **48/48 / 48/48 / 48/48** |
| Plugin sets (turn-only + learned AI; real-time only) | **37/37, 33/33** |
| WebAssembly suites | **15/15** |
| Browser build (`check-web.py`) | **13/13** |
| T82 `Umbra.Authoring` game pack: only game files packed (no concept pictures or notes), shaders under `shaders/`; unpacked files equal the sources; the installed hash equals the pack's; a `../` path and a range past the end are refused and leave nothing marked installed; a missing shaders folder is refused | pass |
| T82 `Umbra.App` touch: hidden until a finger touches (a PC never shows them); stick gives (2/3, 1/3) for a thumb slid 60 px right and 30 px up at 720p, clamped to 1 past its radius, 0 when lifted; camera drag passes the finger's movement; a finger on Jump holds Space until lifted; opening a menu hides the controls and lets go of keys; the fight layout has 1-4 and Flee, not Use; every button lies inside a 1280x720 screen | pass |
| T82 gamepad axes: 0.1 -> 0 (dead zone), 1 -> 1, -0.6 -> -0.5 | pass |
| Android configure (host compiler with `-DANDROID=ON -DANDROID_ABI=x86_64`): only `libmain.so` is generated (no editor, tools, tests); every source compiles; the link stops only at `-landroid -llog`, which exist only in the NDK | as expected |

Picture check (HUD + touch layer rendered on the CPU, 1280x720): exploring shows the stick
with the knob pushed up-right, Keys / Bag / Menu at the top right, Run / Use / Jump at the
bottom right; a real-time fight shows 1-4 in an arc and Flee, over the hotkey bar.

## 2. Not verifiable in the cloud

- Gradle, the Android Gradle plugin and vcpkg's Android build (downloads blocked).
- SDL's Java side with our static `libmain.so` (`JNI_OnLoad` kept with `--undefined`, `SDL_main`
  exported by `SDL_main.h`).
- Vulkan on the emulator, the lifecycle on a real Android system, touch on a real screen.

## 3. Risks to watch on the first run

| Risk | Sign | Fix |
|---|---|---|
| Emulator without host Vulkan | "Vulkan" error in the log at start | Device Manager > Edit > Graphics: Hardware; recent emulator |
| `samplerAnisotropy` missing on the emulator GPU | "sampler anisotropy unavailable" | make anisotropy optional in the renderer |
| Jolt built with AVX2 but the emulator CPU lacks it | crash (illegal instruction) at the first physics step | build Jolt for Android x86_64 without AVX2 (vcpkg overlay) |
| SDL Java and C versions differ | crash at start mentioning a missing native method | copy the Java files from the vcpkg SDL release |

## 4. Windows checklist (user)

1. One-time setup from `plugins/android-export/README.md` (Android Studio, NDK, CMake 3.31+,
   an x86_64 virtual device, `VCPKG_ROOT`).
2. Rebuild the PC game once (`scripts\build-and-run.bat`): the shaders are now built for
   Vulkan 1.1. Check that the PC game still looks the same.
3. Run `scripts\build-android.bat`. The first build takes long (vcpkg builds the libraries).
   Expected: the emulator starts, the game installs and opens in landscape, the log shows
   `Unpacking the game's files` (first start only) and `Assets: ...`.
4. Touch (mouse clicks on the emulator screen count as touches): the start menu answers to taps;
   after the first touch the stick and buttons appear; drag on the left to walk, on the right
   to look; Jump, Use (talk to the elder), Run, Bag, Keys, Menu.
5. A real-time fight: 1-4 and Flee appear; a turn fight: tap Attack -> a move -> an enemy.
6. Gamepad (emulator: Extended controls, or a USB/Bluetooth pad on a phone): left stick
   walks, right stick looks, buttons as on the PC.
7. Leave and return: press Home, wait, open the app again: the game is paused, the picture
   comes back, Continue resumes. Then swipe the app away from the recent apps while exploring
   and start it again: Continue loads the autosave.
8. The back button (or gesture) opens the game menu and does not close the app.
9. Rotate the emulator: the game stays in landscape (flips between the two landscape sides).
10. Report anything odd with `out\android.log` and the game log (the window the script leaves
    open).

Later (P11.5): `scripts\build-android.bat -Abis arm64-v8a` on a phone; 20 minutes of play
for heat and frame time.
