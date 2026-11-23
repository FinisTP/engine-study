# Campaign plan 7: around a stage — the save, the world map, the stage screens and the menus — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The campaign from the title to the end of slice 1, kept in one save: title and new game,
the café hub, the world map with its tickets and refilling skirmishes, the pre-stage screen, the stage
result, game over with the rite, pause and settings.

**Architecture:** `CampaignSave` (version 2, atomic write) and pure progress functions (`WinStage`,
`LoseStage`, `RefillSkirmishes`, `XpFor`, `Unlocked`) in `umbra::campaign`; `TheatreMap` (`map.json`:
rooms as tickets on the floor plan, unlocks, skirmish twists). The app's `CampaignApp` is the one flow;
it owns no rules.

**Spec:** `docs/design/ux/world-map.md`, `ux/stage-flow.md`, `ux/menus.md`, `ux/interaction-patterns.md`
§1; slice design §7.6–7.10. The user, 2026-11-23/24: **tickets and a cursor** (no walking figure); **the
café is the hub**, not a room on the map; **skirmishes refill after each story stage won, each time with
a new twist**; XP from them capped at the next stage's recommended level; **the rite once in the whole
game**, kept used win or lose; no save in a stage (a developer-only save behind `--dev`).

## Global Constraints

- Saving is automatic in the café and on the map; a lost stage keeps nothing (back to the save from
  before it), except the rite's use. A corrupt save is reported and kept; a new game over a save asks to
  confirm.
- Title: Tiếp tục (with a save, focused, lands in the café), Trò chơi mới, Cài đặt, Thoát. New game: a
  name (1–16 letters, Vietnamese allowed) and Nam / Nữ; both fill `{player}` and `{anh/chị}`.
- World map: tickets carry a name and one mark (✓ cleared, ◆ next, ⚔ skirmish, ? unknown); details in
  the selected-room panel; a skirmish's panel names its current twist; the cursor jumps to the nearest
  ticket; Back returns to the café in one press; the map opens on the next stage or the room just played.
- Pre-stage: who fights (the player locked, the stage's party cap), the revive warning, "Sửa trong Sổ
  tay"; two presses from the map to a fight. Stage result: rewards, level-ups, found items, ingredients,
  keepsakes, freed; Về quán focused when a brew or talk waits. Game over: the reason line, Lễ cầu hồn
  only for a player fall, while unused and after T3 (confirm, focus on Không), Thử lại one press away,
  T1 only Thử lại.
- Pause: no Save; in a stage Bỏ cuộc (confirm); elsewhere Về màn hình chính, Thoát game. Settings: full
  screen, Cỡ chữ, Màn hình TV cũ, Băng mòn (moved from pause), music and effects volume, text speed,
  enemy speed, language. Changes apply at once and save.
- Mouse and keyboard complete; gamepad later (plan 8). Commits from **#379**.

## Content: the theatre map (slice 1)

Floor plan (`map/floor_plan.png` from the art set; a placeholder until then): Tiền sảnh (T1), Hành lang
chân dung (T3), Phòng vé (Stage 1), Kho đạo cụ, Cầu thang phụ and Phòng vé (skirmishes after T3 / after
Stage 1), and dark "?" tickets for the rooms to come (Phòng hóa trang, Hầm sân khấu, Ban công, Phòng
nhạc, Gác thượng, Sân khấu chính), each known-but-locked one with what opens it.

---

### Task 1: The save (#379)

`CampaignSave` v2: player (name, gender); party (id, level, xp, slots, chosen abilities and versions,
boosts used); storage; recipes; memories; facts; rooms (cleared, items taken, refill index); chapter;
`riteUnlocked`, `riteUsed` (once per game); last party; the café's lines worded by the model. Save and
load with a temp file and rename. Tests: round trip; wrong version refused; corrupt kept; interrupted write
doesn't replace the good file. Commit.

### Task 2: Progress (#380)

`WinStage` (xp with the skirmish cap, found things to storage, items to slots then storage, room cleared,
facts, chapter, recruits freed), `LoseStage` (nothing kept but the rite's use), `RefillSkirmishes` (after
each story win: refill index +1, taken drops come back), `XpFor`, `Unlocked`. Tests: the cap; a refill
changes the twist; a lost stage keeps nothing; the rite stays used after a loss. Commit.

### Task 3: The world map (#381)

`map.json` (tickets, `at` on the plan, kind, stage or skirmish id, opensAfter, lock line); `TheatreMap`
validation; `worldmap.ui.json` (tickets, the panel, Về quán with the café's news line); the cursor's
nearest-ticket move; new-room fade and the skirmish pulse with toasts. Tests: openings in order; every
ticket reachable with the arrows; panels per kind; acceptance criteria of `world-map.md`. Capture. Commit.

### Task 4: Pre-stage, stage result, game over (#382)

Three screens and their data; the revive warning rule; the reason lines; the rite's box rules. Tests per
acceptance criterion in `stage-flow.md`. Captures. Commit.

### Task 5: Title, new game, pause, settings (#383)

Screens per `menus.md`; the name field (the engine's `field` exists); settings in prefs; the analog switches
move from the old pause to Settings (the career's pause keeps its own). Tests. Captures. Commit.

### Task 6: The one flow (#384)

`CampaignApp`: title → (new: the prologue) → café hub → map → pre-stage → fight (scenes inside) → result
→ café or map; game over choices; `project.json` `"play"`: "Campaign: continue" and "Campaign: new game".
Developer flags `--stage`, `--save`, `--dev` (mid-stage save). Tests: a scripted walk through the slice with
stand-in fights ends with Tí in the party, memories 1–3 possible, the skirmishes refilled, the save written.
Commit.

### Task 7: Records (#385)

`docs/testing/s51-campaign-flow.md` (§4 on Windows: new game with a Vietnamese name, the whole slice,
quit and continue, each game-over choice, the rite once, a skirmish after the cap gives no XP, the refill
twist), ROADMAP, status, CLAUDE.md. Ship.

## After plan 7

Slice 1 plays end to end. Plan 8 (input) follows; then the user plays the slice and picks stage 2's
recruit and its one new idea (the escalation ladder in `Campaign.md`).
