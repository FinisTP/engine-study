# ADR 0009: Android port (P11)

- Status: **accepted; implemented, emulator run open** (2026-11-08).
- Context: roadmap P11 (mobile). The user chose the target and the controls (below). The
  cloud workspace cannot download the Android SDK/NDK, so the Android build itself runs on the
  user's Windows PC with Android Studio.

## Decisions (user, 2026-11-08)

| Choice | Decision |
|---|---|
| Platform | Android |
| First test target | the Android emulator (x86_64) on the PC; phones (arm64) after |
| Controls | touch and gamepad |
| Orientation | landscape only |

## Design

| Topic | Choice | Why |
|---|---|---|
| Graphics | the existing Vulkan renderer; shaders built for Vulkan 1.1 | no second renderer; every Android 10+ device with Vulkan has 1.1 |
| App shell | SDL3's own Java activity (`SDLActivity`), vendored unchanged from the SDL release vcpkg pins (3.4.14) | SDL already handles window, touch, gamepads, audio, lifecycle on Android |
| Native library | the game as `libmain.so`, SDL linked in statically (static vcpkg triplets) | one library to load; the same vcpkg baseline as the PC |
| Build | Gradle (Android Studio's) runs CMake on the repository root through vcpkg chained with the NDK toolchain; a separate small vcpkg manifest for the game's libraries | the normal Android way; no editor or tool libraries built for Android |
| Assets | one `game.pack` (assets + shaders) inside the APK, copied out on first start, re-copied when its hash changes | APK files are not normal files; the rest of the game keeps using file paths unchanged |
| Lifecycle | save + pause + release the Vulkan surface on "entering background"; new surface on return; SDL told not to freeze the loop in the background | Android destroys the window and may end background apps |
| Touch | floating stick (left), camera drag (right), round buttons pressing the existing keys; hotkeys 1-4 and Flee in real-time fights | every screen and rule works unchanged; menus are touched directly |
| Gamepad | left stick moves, right stick turns the camera; buttons as on the PC | the same gamepad code as the PC |
| Where the Android files live | a tool plugin, `plugins/android-export` (like `web-export`) | platform exports are optional per project |

## Rejected

- **OpenGL ES renderer**: a second renderer to keep in step; Vulkan 1.1 covers the devices.
- **NativeActivity / GameActivity without SDL**: would duplicate what SDL already does
  (input, audio, lifecycle) and split the platform layer.
- **Reading assets through the Android asset manager everywhere**: every loader would need
  a second path; unpacking once keeps one file-based code path (the pack is about 4 MB).
- **Shared SDL library**: works, but static linking is what the vcpkg Android triplets give
  and keeps one library.

## Consequences

- The PC build makes the shaders; an Android build needs a PC build first.
- When the vcpkg SDL version changes, the Java files must be copied again from that release.
- The first Android build compiles the libraries with vcpkg (slow once, cached after).
- Not verified in the cloud: Gradle, the NDK build and the emulator run happen on Windows
  (`docs/testing/p11-android.md`).
