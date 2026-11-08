# Android export (roadmap P11)

The Android app: the same game as on the PC, as an APK for the **Android emulator** (first
target) and **phones** (arm64). Landscape only; touch or gamepad. How the game behaves on a
phone (pack, lifecycle, touch math): `apps/umbra/Mobile.md`. Decisions: `docs/adr/0009-android.md`.

## One-time setup (Windows)

1. Install **Android Studio**. Open it once and finish its setup (it installs the Android SDK).
2. **Tools > SDK Manager > SDK Tools**, tick **Show Package Details**, then install:
   - **NDK (Side by side)**: the newest version (28 or newer).
   - **CMake**: 3.31 or newer (the engine needs 3.28 or newer).
3. **Tools > Device Manager > Create virtual device**: any phone, a system image with
   **x86_64** and API 30 or newer (Google APIs image is fine). Start it once from there to
   check that it runs.
4. `VCPKG_ROOT` must point to vcpkg, as for the PC build.
5. Build the PC game once (`scripts\build-and-run.bat`): the Android build takes `UmbraTool`
   and the compiled shaders from `out\build\windows-msvc`.

## Build and run

Double-click `scripts\build-android.bat`. It:

1. finds the SDK, the newest NDK and CMake, and Android Studio's Java;
2. packs the project's assets and the shaders into `game.pack` (`UmbraTool game-pack`);
3. builds the APK with Gradle (`android\gradlew.bat assembleDebug`). The **first build also
   builds the libraries for Android with vcpkg** (SDL3, Jolt, spdlog, ...): it takes a while.
   Later builds reuse them;
4. starts the first virtual device if no emulator or phone is connected, installs the APK,
   starts the game and shows its log (Ctrl+C ends the log, not the game).

Options: `scripts\build-android.bat -Abis arm64-v8a` for a phone (USB debugging on),
`-Abis "x86_64,arm64-v8a"` for both, `-Project <name>`, `-NoRun` (APK only). The log of the
Gradle step is `out\android.log`; the APK is
`plugins\android-export\android\app\build\outputs\apk\debug\app-debug.apk`.

You can also open the `android` folder in Android Studio (build, run, debug C++). Run
`build-android.bat -NoRun` once before, so `game.pack` exists.

## What is in this folder

| Path | What it is |
|---|---|
| `android/` | the Gradle project (app id `com.umbra.game`, min Android 10, target 35) |
| `android/app/build.gradle` | CMake on the repository root through vcpkg + the NDK; `game.pack` as an app asset |
| `android/app/src/main/AndroidManifest.xml` | landscape, Vulkan 1.1 required, SDL settings |
| `android/app/src/main/java/org/libsdl/app/` | SDL's Java activity, unchanged from SDL `release-3.4.14` (zlib license, `LICENSE.txt`); must match the vcpkg SDL version |
| `android/app/src/main/java/com/umbra/game/UmbraActivity.java` | the game's activity: loads only `libmain.so` |
| `android/vcpkg/vcpkg.json` | the libraries the game needs (no editor, test or AI tool libraries), same baseline as the PC |
| `android/gradlew*`, `android/gradle/wrapper/` | Gradle wrapper (from SDL's Android project) |
| `tools/build-android.ps1` | the script above |

In CMake, `ANDROID` builds only the engine modules, the game plugins and the game, as the
shared library `libmain.so` (root `CMakeLists.txt`, `apps/umbra/CMakeLists.txt`). The vcpkg
triplet follows the ABI: `x86_64` -> `x64-android`, `arm64-v8a` -> `arm64-android`.

## When the vcpkg SDL version changes

Copy the Java files again from the matching SDL release
(`android-project/app/src/main/java/org/libsdl/app/` of tag `release-<version>`): the Java
and C sides of SDL must be the same version.

## Problems

| Symptom | Fix |
|---|---|
| "no NDK" / "no CMake 3.28 or newer" | SDK Manager, step 2 above |
| vcpkg fails at the first build | read `out\android.log`; `ANDROID_NDK_HOME` is set by the script to the NDK it found |
| `INSTALL_FAILED_NO_MATCHING_ABIS` | an arm64 APK on the x86_64 emulator: use `-Abis x86_64` |
| The app closes at once | the log shows the reason (tag `Umbra`). "Vulkan" errors: the emulator needs a Vulkan-capable host GPU (Device Manager > Edit > Graphics: Hardware) |
| Black screen after returning to the app | report it with the log: the surface restore failed |
