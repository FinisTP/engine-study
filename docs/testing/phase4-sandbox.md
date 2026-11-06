# Phase 4 — Playable 3D sandbox: verification record

## Record

- Date: 2026-11-06
- Implemented in commits `issue #UMBRA-21` (P4.1) through `issue #UMBRA-25` (P4.5), on `main`
- Tester: Claude (remote session) for automated Linux checks. **Windows, GPU, audio and interactive checks are pending the user.**
- State: **implemented; headless parts verified on Linux; Windows/GPU/interactive gate open**

User decisions used (2026-11-06): third-person orbit camera; Jolt behind an engine-owned adapter; procedural animation only (no skeleton); W0 web experiment after P4.

## 1. Automated checks run (Linux, cloud workspace)

Environment as in [`linux-headless-build.md`](linux-headless-build.md), plus JoltPhysics 5.6.0 built from source (Debug). Debug builds of the whole tree.

| Configuration | Result |
|---|---|
| GCC 13.3, `-Wall -Wextra -Wpedantic` | Build OK, 0 new warnings, 18/18 CTest entries pass |
| Clang 18 | Build OK, 0 new warnings, 18/18 pass |
| GCC 13.3 + `-fsanitize=address,undefined` (LeakSanitizer on) | Build OK, 18/18 pass, no sanitizer or leak reports |

New CTest entries in P4: `Umbra.Physics`, `Umbra.Gameplay`, `Umbra.Audio`, `Umbra.SceneCheck.Settlement`, `Umbra.SceneCheck.Dungeon`. `Umbra.HeadlessDependencies` now also covers Physics, Gameplay and Audio (none may link SDL3, Vulkan, ImGui or the renderer).

**Limits of this evidence:**

- The `Umbra` app and renderer **compile**; nothing was drawn on a GPU and no sound was heard.
- The audio device test uses SDL's dummy driver (`SDL_AUDIO_DRIVER=dummy`, set by CTest), so it proves the callback path, not real output.
- Mouse capture, focus loss and the pause menu were checked only through their state classes and code review, not by playing.
- MSVC, clang-cl and MSVC-ASan were **not run**.

## 2. Roadmap test cases (P4 gate)

| Case | Evidence | Status |
|---|---|---|
| Walk, collide, jump, stairs, ramp | `SettlementWalkTests` on the real settlement scene | Passed (headless) |
| Interact | `SettlementWalkTests` (signpost), `AreaTests` (dungeon chest), scene `interactable` tests | Passed (headless); E key/HUD **not run** |
| Restart / transitions / checkpoint | `AreaTests` (10 settlement↔dungeon trips, constant body/character/entity counts), checkpoint round trips byte-identical, broken checkpoints change nothing | Passed (headless) |
| T05 focus loss during movement | Focus loss clears input (`InputTests`) and opens the pause menu; resume uses one normal frame delta | Logic passed; interactive **not run** |
| T44 replaced resource still in use | P3 retire queue unchanged; area changes replace World/PhysicsWorld whole, mixer voices keep their clips alive (`AudioTests` "keeps its clip alive") | Logic passed; GPU validation **not run** |
| T56 pause/lifecycle | Pause stops simulation and game sounds (UI sounds continue, `AudioTests`); fixed step caps catch-up at 5 steps; suspend resets the clock | Logic passed; interactive **not run** |
| Missing asset diagnostics | Scene errors/warnings and failed sounds listed on screen; `entity.interactable.sound` warning in `UmbraSceneCheck`; `sound.load` reported once (`AudioTests`) | Headless passed; on-screen **not seen** |

## 3. Baseline measurements (Linux cloud VM, not a reference machine)

`UmbraGameplayTests "[benchmark]"` (hidden test; the CTest run skips it), 2 vCPU Intel Xeon, `-O2` engine code, Jolt built Debug:

| Measurement | Result |
|---|---|
| One fixed step: player + physics + transforms, settlement | ≈ 0.018 ms |
| Load an area (scene parse + bodies + spawn), settlement/dungeon average | ≈ 0.49 ms |

These are a smoke baseline only; they say nothing about frame rate on the target PC. Note: an `NDEBUG` build against a Debug-only Jolt crashes (Jolt's assert-dependent layouts differ). On Linux, measure with a custom config using `-O2 -UNDEBUG`; vcpkg on Windows builds matching Debug and Release Jolt libraries, so this does not apply there.

## 4. Windows gate (to run, then fill in)

From an x64 Visual Studio Developer PowerShell with the Vulkan SDK and `VCPKG_ROOT`:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug          # or: ctest --test-dir out\build\windows-msvc -C Debug
& ".\out\build\windows-msvc\apps\umbra\Debug\Umbra.exe"
& ".\out\build\windows-msvc\tests\gameplay\Release\UmbraGameplayTests.exe" "[benchmark]" -s   # after a Release build
```

`vcpkg.json` gained `joltphysics` in P4.1; expect vcpkg to build it once.

- [ ] Configure, build and all 18 CTest entries pass (MSVC Debug)
- [ ] Same with `windows-clang` and `windows-msvc-asan`
- [ ] Release benchmark numbers recorded in section 3

## 5. Interactive checklist

Run `Umbra.exe` (default scene `scenes/settlement.scene.json`) with Vulkan validation on.

**Movement and camera (P4.2)**

- [ ] The mouse is captured; moving it orbits the camera; WASD moves relative to the camera; diagonals are not faster; Shift sprints; Space jumps
- [ ] The 1 m wall ahead blocks walking and can be jumped over; the stairs on the right and the ramp on the left can be walked up
- [ ] Backing the camera into a house pulls it in front of the wall instead of through it
- [ ] F1 shows collider outlines (gray static, green dynamic, yellow player capsule); pushing the crates moves them

**Presentation (P4.3)**

- [ ] The body bobs once per step and leans forward with speed; footsteps are heard in time; jumping plays a sound
- [ ] Facing the signpost (behind the spawn) shows `[E] Read sign`; E shows the message for ~3 s with a chime; the well works too
- [ ] Esc opens the pause menu: the world freezes, footsteps stop, the menu click is heard; W/S moves, Enter chooses; Resume, Restart area and Quit work
- [ ] Alt-Tab away pauses the game and releases the mouse; nothing keeps moving on return
- [ ] With no audio device (disable it), the game runs and logs "game runs silently"
- [ ] Rename `assets/audio/jump.wav`, jump: one warning appears top-right, the game continues. Restore it

**Areas and checkpoints (P4.4)**

- [ ] Walking east into the purple gate loads the dungeon; walking back through its exit returns to the settlement next to the gate (no bouncing)
- [ ] The dungeon chest shows `[E] Open chest`
- [ ] F5 shows "Checkpoint saved"; push a crate, walk away, F9 restores crate and player; this also works after changing area
- [ ] R restarts the area at its spawn
- [ ] Go through the portals 20+ times: the overlay's bodies/entities return to the same numbers each time and memory in Task Manager stays flat
- [ ] No validation errors during any area change, checkpoint load or restart

**Observability (P4.5)**

- [ ] The diagnostics window shows Frame/Reload/Simulate/Extract/Render average and max in ms, physics steps, bodies, characters, entities, models, sounds and the current area
- [ ] Record the steady-state numbers in section 3 (note resolution and GPU)
- [ ] A RenderDoc capture of a settlement frame shows the scene draws plus one overlay draw list

## 6. Known gaps (deliberately left for later)

- No skeletal animation (user decision); the bob/lean is applied to one rigid body mesh.
- Audio: one fixed WAV format, no streaming, no 3D positioning, no music; menus are keyboard-only (ImGui receives no input).
- Portals use the entity box, not physics triggers. Checkpoints are development tools, not the P6 save system (no versioning beyond `version: 1`, no slots).
- Loaded models stay cached for the whole run, so GPU memory grows with the number of distinct models visited (bounded by content, not by trips).
- No process-memory counter in the overlay; use Task Manager / ASan / LSan.
