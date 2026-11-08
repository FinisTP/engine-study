# The game on phones (roadmap P11)

The same game runs on Android: the same C++, Vulkan renderer, assets and saves. This page
explains what is different on a phone. Building the app: `plugins/android-export/README.md`.
Why it is done this way: `docs/adr/0009-android.md`.

## 1. How the app starts

```text
Android starts UmbraActivity (SDL's Java activity)
  -> loads libmain.so (the game, SDL linked in statically)
  -> calls SDL_main = our main() (SDL_main.h renames it on Android)
  -> UnpackPackagedGame(): game.pack from the APK -> the app's own folder (first start only)
  -> Platform::SetProgramFolder(that folder): FindFolders finds assets/, the renderer shaders/
  -> the normal game loop
```

An APK is a zip file: the game cannot open its files with normal file paths. So the assets
and shaders travel as one file, `game.pack`, and are copied out once:

| File | What it is |
|---|---|
| `game.pack` | `assets/...` and `shaders/*.spv`, back to back |
| `game.pack.json` | where each file sits in `game.pack` (offset, size), and a hash of the whole list |

`UmbraTool game-pack <project assets> <shaders folder> <out folder>` writes both
(`engine/authoring/src/WebPack.cpp`, the same writer as the browser's `web-pack`).
At start (`engine/assets/src/GamePack.cpp`):

1. Read `game.pack.json` from the APK (`SDL_LoadFile` reads APK files).
2. The target is `<app data>/game`. If its `.pack-hash` equals the hash of this index, the
   files are already there: done. (A new app version with other assets has another hash.)
3. Otherwise delete `.pack-hash` first, so a copy cut short (battery, crash) is redone next
   time, then write every file. Each path is checked (`IsSafeAssetPath`: no `..`, no absolute
   path) and each range must lie inside `game.pack`.
4. Write `.pack-hash` last.

Saves are not in that folder's way: they live in the preferences folder next to it and are
never overwritten by unpacking.

The shaders are compiled on the PC (DXC, Vulkan SDK) for **Vulkan 1.1** (SPIR-V 1.3), so the
same `.spv` files work on the PC, phones and the Android emulator. The Android build has no
shader compiler.

## 2. Leaving and returning to the screen

On a phone the system may take the window away at any time (home button, call, screen off)
and may end a background app without asking. The game therefore:

| SDL event | Game |
|---|---|
| `SDL_EVENT_WILL_ENTER_BACKGROUND` | save the autosave slot (only while exploring: `GameFlow::SaveForSuspend`), pause, `Renderer::ReleaseSurface()`, idle |
| `SDL_EVENT_DID_ENTER_FOREGROUND` | `Renderer::RestoreSurface()`, the paused menu is shown |
| `SDL_EVENT_TERMINATING` | save, quit |
| `VK_ERROR_SURFACE_LOST_KHR` on a frame | the window went first: release the surface the same way |

`ReleaseSurface` waits for the GPU, then destroys what draws into the window (overlay,
per-frame resources, pipeline, swapchain, surface). Meshes and textures stay in GPU memory,
so returning is quick. `RestoreSurface` makes a new surface for the window Android gives
back; the next frame builds the swapchain on it like after a resize.

SDL normally freezes the game loop while the app is in the background, and the loop would
only see "entering background" after coming back: too late to save. The manifest turns that
off (`SDL_ANDROID_BLOCK_ON_PAUSE=0`); the loop sleeps 100 ms per turn while suspended.

The back button/gesture is the Escape key (`SDL_ANDROID_TRAP_BACK_BUTTON=1`): it opens the
game menu instead of closing the app.

## 3. Touch controls (`TouchControls.cpp`)

The controls appear after the first touch, only while the game has the screen (not over
menus or the turn menu, which are touched directly: a touch is also a mouse click).

```text
+--------------------------------------------------------------+
|                                          (Keys) (Bag) (Menu)  |
|                                                               |
|   left 45%: movement stick        the rest: drag = camera     |
|                                                       (Run)   |
|     ( stick )                              (Use)   (Jump)     |
+--------------------------------------------------------------+
real-time fight: (1) (2) (3) (4) in an arc in the lower right corner, (Flee) above
```

Sizes scale with the screen height: `s = height / 720`, so a button covers the same share of
the screen on any phone. Buttons press the same keys as the keyboard (`HandleKeyDown`), so
every screen and rule works unchanged.

**Floating stick.** A finger that lands in the left 45% (not on a button) becomes the stick;
where it landed is the stick's centre `c`. With the finger at `p` and the radius
`R = 90 * s`:

```text
d    = p - c
d'   = d                 if |d| <= R
       d * (R / |d|)     otherwise          (kept inside the circle)
move = (d'.x / R, -d'.y / R)                (-1..1; screen y grows downwards, so it is flipped)
```

**Camera.** A finger that lands right of the stick area (and not on a button) turns the camera by its
movement since the last frame: `look = (dx, dy) * 0.006` radians per pixel.

**Gamepad.** The left stick moves and the right stick turns the camera at `2.5` rad/s at full
tilt. Small stick values are ignored (dead zone `0.2`) and the rest is stretched back to 0..1:

```text
v' = 0                              if |v| < 0.2
     sign(v) * (|v| - 0.2) / 0.8    otherwise
```

Touch, gamepad and keyboard add up, so any of them can be used at any time. A touch also
moves the mouse pointer (for the menus) but never turns the camera like a mouse would.

## 4. Limits

- Landscape only (`sensorLandscape`: both landscape sides).
- Vulkan 1.1 or newer is required (every Android 10+ device with Vulkan); no OpenGL ES path.
- No editor, AI tools or Python on the phone; the learned enemy runs as on the PC.
- Battery, heat and long-run frame time are measured on real phones only
  (`docs/testing/p11-android.md`).
