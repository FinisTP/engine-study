# Campaign plan 7: the campaign on screen — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the campaign's fights read at a glance: who is strung and to where, who has fallen, who is a boss, how much AP each holds, what the enemy will do next round; plus the forms and tints of characters, stage backdrops and the Vietnamese words everywhere.

**Architecture:** Drawing only. `modules/board` gains small pure functions that turn a `MatchState` into draw lists (`Strings`, `Plates`, `ApPips`, `Marks`); the app draws them with the existing renderer and UI. No rule changes; tests check the lists, captures check the look.

**Tech Stack:** C++20, Catch2; `board`, `ui`, the renderer (Vulkan and WebGPU kept in step).

**Spec:** `docs/design/2026-11-23-campaign-slice-1.md` §7.12; `art-slices-2026-11-23.md`; the analog screen (#333) stays on top of everything.

## Global Constraints

- No blood, gore, skulls or bodies. A fallen unit **kneels and dims** (alpha 0.5, desaturated), never lies broken.
- Strings: red, from the unit's head up to an **anchor** in the ceiling (the stage file names anchors; default straight up). A freed unit's string snaps and its knot turns **gold**, then the gold light lifts it away (the user, 2026-11-23).
- Bosses: a small crown-less mark — a theatre mask outline — above the plate. Recruits: a red knot on the plate.
- AP: up to the cap, small pips under the plate; spent pips grey out during a burst.
- Telegraphs (the Keeper's breath, windows, sandbags, reinforcements): marks drawn **a round ahead**, the same red-orange as the order strip's "spawns soon".
- Every word on screen from `campaign-words.json` / `match-words.json`, `vi` and `en`; `ui::MissingGlyphs` finds none in the slice's screens.
- Keep `Scene.hlsl` / `Post.hlsl` and the WGSL in `umbra_render.js` in step if any shader changes (aim: none).
- Commits from **#371**.

---

### Task 1: Forms and tints (#371)

The player's figure takes the chosen form (two guest meshes exist; pick by `form`), tints from `cast.csv` (`tint` column) for party and enemies; ordinary ghosts at height 0.8 and a cold tint. Tests: the draw list's mesh and tint per unit. Capture. Commit.

### Task 2: Strings, knots and the gold lift (#372)

`Strings(state, anchors) -> lines` (strung units only); the snap and lift as a short animation driven by the `Freed` log entry (0.8 s), the unit removed from the drawn board after it. Tests: only strung units; a freed unit's line gone and its lift queued. Capture. Commit.

### Task 3: Plates, falls and AP (#373)

`Plates(state)` (name, HP bar, boss mask, recruit knot, AP pips), fallen pose and dim; plates stay readable over the analog screen (test the contrast on a capture). Tests per field. Commit.

### Task 4: Telegraph marks (#374)

`Marks(state, stage)` for the next round's breath lane, opening windows, falling sandbags, reinforcement squares; a short legend line on first sight (from the stage's teaching beats). Tests per gimmick. Capture. Commit.

### Task 5: Backdrops and the order strip's faces (#375)

Stage backdrops from the stage file (placeholders until the art exists; requests in `art-requests.md`: foyer, portrait corridor, box office, prop store, back stairs, the theatre map); enemy portraits for named enemies (the Rat, the Keeper) once drawn, the name-only fallback kept. Commit.

### Task 6: Words and records (#376)

Every new screen string through the word files; `MissingGlyphs` over all campaign screens in a test; `docs/testing/s51-campaign-on-screen.md` (§4: play Stage 1 on Windows with the analog screen on and off and judge readability), ROADMAP, status. Ship.

## After plan 7

The slice is whole: title → T1 → café → T3 → skirmishes → Stage 1 → Tí in the café. The next design step is the user's: play it, then choose stage 2's recruit and its one new idea from the escalation ladder in plan 4.
