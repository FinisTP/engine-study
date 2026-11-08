# Phase 7 — Creator workflow, validation and iteration: verification record

## Record

- Date: 2026-11-07
- Implemented in commits `issue #UMBRA-44` (P7.3) through `issue #UMBRA-49` (P7.6), on `main`
- Tester: Claude (remote session), automated Linux checks only
- State: **implemented; headless parts verified on Linux; the editor app compiles but has not
  been run.** The Windows build, the editor walkthrough and the package test are open
  (sections 3–4).

User decisions (2026-11-07):

- A separate editor program.
- Number fields plus a mouse gizmo (ImGuizmo).
- Forms for RPG content.
- Packaging as a folder plus zip.

Order of work: P7.3 (shared edits) and P7.4 (tool) came first, because the editor and the forms
are built on them.

## 1. Automated checks run (Linux, cloud workspace)

| Configuration | Result |
|---|---|
| GCC 13.3, `-Wall -Wextra -Wpedantic` | Build OK (engine, tests, game, editor, tools), 0 warnings, 32/32 CTest entries pass |
| Clang 18 | Build OK, 32/32 pass |
| GCC 13.3 + ASan/UBSan | Build OK, 32/32 pass, no reports |

New CTest entries:

- `Umbra.Authoring`: Document, SceneEdits and Validate tests.
- `Umbra.Tool.ValidateShipped`, `Umbra.Tool.ValidateBroken` (must fail), `Umbra.Tool.Fight`.
- `Umbra.GameExcludesEditor`.

`Umbra.HeadlessDependencies` now also covers `Umbra::Authoring` and `UmbraTool`.

The editor's new libraries (ImGui SDL3 backend, ImGuizmo 1.10) were built from source in the
Linux harness. On Windows they come from vcpkg (`imgui[sdl3-binding]`, `imguizmo`). The first
configure after `#UMBRA-46` downloads and builds them.

## 2. Roadmap evidence and test cases

| Task / case | Evidence | Status |
|---|---|---|
| P7.1 runtime excludes editor | `Umbra.GameExcludesEditor`: the game links no `UmbraEditor`, `UmbraAuthoring` or `imguizmo` | Passed |
| P7.2 item/skill/quest by data only | `DocumentTests`: a new item and a quest using it are added through the document; the game loader reads them; no C++ rebuild | Passed |
| P7.3 redo/undo, conflict | `DocumentTests`: undo/redo, merged drag steps, stale `expectedRevision` refused, outside file change blocks save | Passed |
| P7.4 matching diagnostics | `ValidateTests`: the same planted error gives identical codes/fields from the tool and the editor document | Passed |
| T22 failed build + known-good | `build-and-run.bat` copies a fully passing build plus its assets to `out\known-good` and points to it when a build fails | Script written; Windows run open |
| T37 non-ASCII text and paths | `DocumentTests`: a UTF-8 name in a non-ASCII folder survives edit, save and game load; the editor converts paths as UTF-8 | Passed (data). The editor font has no Vietnamese/CJK glyphs yet (P9). |
| T42 imported text asks for execution | Content is only ever parsed as JSON and checked; no authoring or tool path runs commands | Passed (by design/inspection) |
| T48 stale base, repeated undo/redo | `DocumentTests` (revisions name states; save conflict) | Passed |
| T51 reload changes an active quest/skill | Game reloads RPG content only while exploring, after checking the running game against it like a save; `GameSessionTests` covers the check | Passed (rules); live run open |
| T53 game/headless leak editor deps | `Umbra.GameExcludesEditor`, `Umbra.HeadlessDependencies` | Passed |
| T58 authoring falls back to cloud | No network code in authoring, editor or tool | Passed (by inspection) |

## 3. Iteration measurements

Linux harness: 2 cores, GCC Debug, Ninja.

| Edit | Rebuild time |
|---|---|
| No-op build of the game | 0.01 s |
| A gameplay file (`apps/umbra/GameFlow.cpp`) | 3.3 s |
| An engine rules file (`game/rpg/src/Rules.cpp`) | 2.0 s |
| An editor file (`apps/editor/RpgForms.cpp`) | 4.3 s |
| A shared header (`engine/core/include/DrawList.hpp`), everything rebuilds | 39.7 s |
| `UmbraTool validate` of the RPG content / dungeon with models | 0.01 s each |

Data edits need no rebuild. Scenes and models reload in the running game. RPG content reloads at
the next safe moment, and the game shows the time it took.

To fill in on Windows (MSVC Release, user's PC):

| Edit | Time | Result |
|---|---|---|
| `GameFlow.cpp` touched, `build-and-run.bat` | | |
| `DrawList.hpp` touched | | |
| Item edited in the editor, Ctrl+S, game running | | message "RPG content reloaded (… ms)" |

## 4. Windows checklist (open, needs a person at the PC)

| # | Do | Expect | Result |
|---|---|---|---|
| 1 | `scripts\build-and-run.bat` | Build OK, 33 CTest entries pass (32 + the Windows-exception test), `out\known-good` written | |
| 2 | `scripts\editor.bat` | Editor opens on the settlement: hierarchy, inspector, assets, grid | |
| 3 | Click the signpost; W and drag; Ctrl+Z | It moves; one undo puts it back | |
| 4 | Inspector: type a duplicate id (`floor`) | Refused; Diagnostics shows the reason | |
| 5 | Follow `docs/creator-guide.md` section 2 (bread item, give to hero) and press F5 | Game starts; the hero has 2 Bread | |
| 6 | With the game running, change Bread's heal power and Ctrl+S | Game shows "RPG content reloaded" | |
| 7 | Guide section 3 (planted error) | `UmbraTool validate` prints `ref.item … helth_potion`; editor refuses to open it the same way | |
| 8 | Break a `.cpp` on purpose and run `build-and-run.bat` | BUILD FAILED, and the log points to `out\known-good\Umbra.exe`, which still runs | |
| 9 | `scripts\package.bat`, copy `out\package\Umbra.zip` to another folder (ideally another PC), run `Umbra.exe` | The game runs from the package with its own assets | |
| 10 | A second person follows the guide without help (roadmap exit gate) | Done without undocumented fixes | |

## 5. Known limits

- The editor does not reload model files while it runs; reopen the scene.
- Conversation conditions and actions are edited as small JSON lists (the rest of the forms are
  typed fields).
- The editor's font covers Latin only; non-Latin text is stored correctly but shows as `?`.
- One battle arena for every area (P6).
