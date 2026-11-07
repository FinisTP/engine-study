# Phase 6 — Complete offline reference RPG: verification record

## Record

- Date: 2026-11-07
- Implemented in commits `issue #UMBRA-37` (P6.1) through `issue #UMBRA-42` (P6.5), on `main`;
  play-test fixes in `issue #UMBRA-43`
- Tester: Claude (remote session), automated Linux checks only
- State: **implemented; rules verified headlessly on Linux; the playable game (P6.4b) is
  compile-checked only.** The Windows build and the play-through below are open.

User decisions used (2026-11-07):

- Dungeon areas have enemy pools. Enemies roam, and some chase on sight. Touching one starts
  a fight with 1–3 enemies. After the first play test (below) every fight moves to a separate
  battle arena scene and returns to the same dungeon spot.
- Defeated enemies stay gone until the area is entered again.
- Turn-based (default) or real-time is chosen outside combat only.
- Fleeing always works.
- Quests can fail (for example on death).
- Saving: E on a save stone writes the one manual slot, which keeps a backup. Changing area
  writes the one autosave slot.
- On death the menu offers: Load save, Load last autosave, New game.
- The hero grows by XP and levels.
- The quest is a placeholder written by Claude ("The Stolen Relic", Elder Mara).

## 1. Automated checks run (Linux, cloud workspace)

| Configuration | Result |
|---|---|
| GCC 13.3, `-Wall -Wextra -Wpedantic` | Build OK (engine, tests and the `Umbra` app), 0 new warnings, 27/27 CTest entries pass (after `#UMBRA-43`; Windows has one more, the Windows-exception test) |
| Clang 18 | Build OK, 27/27 pass |
| GCC 13.3 + ASan/UBSan | Build OK, 27/27 pass, no reports |

New CTest entries: `Umbra.Story` and `Umbra.Game`. `Umbra.HeadlessDependencies` also covers
Story and Game, which link no window, GPU or audio.

## 2. Roadmap test cases

| Case | Evidence | Status |
|---|---|---|
| T13 reward doesn't fit | Kept in `pending`, claimed later (`QuestTests`, `ProgressionTests`) | Passed |
| T14 early or repeated quest events | Early events ignored; reward once (`QuestTests`) | Passed |
| T15 conversation interrupted | Cancel/walk away changes nothing; all-or-nothing choices (`ConversationTests`) | Passed |
| T16 save in an unsafe moment | `MakeSave` refuses during fights and conversations (`GameSessionTests`) | Passed |
| T17 failed write | The previous save stays; no false success (`SaveGameTests`) | Passed |
| T18 older/newer/foreign save | Newer refused, foreign/damaged refused; the backup rescues a damaged file (`SaveGameTests`) | Passed |
| T19 decision after a load | A decision for the fight that a load ended is refused (`GameSessionTests`) | Passed |
| T49 identity after reload | Saves hold ids and counts only; all checked against content (`SaveGameTests`) | Passed |
| Retry pays once | Reload, then win again: the same XP, one relic (`GameSessionTests`) | Passed |
| Death fails the quest | `failOn: death` quests fail; loading the autosave restores them (`QuestTests`, `GameSessionTests`) | Passed |
| Offline | No network code anywhere in `engine/` or `apps/` (searched for sockets, HTTP, curl, SDL_net: none) | Passed (by inspection) |

## 3. Windows checks

2026-11-07 14:06, user's PC, `scripts\build-and-run.bat` (MSVC Release): build OK with no
compiler warnings in the log, **27/27 CTest entries pass** (`Umbra.Assets` started this time),
and the game opened. That run was before the arena change (`#UMBRA-43`), which adds
`Umbra.SceneCheck.Arena` (28 entries) and still needs a Windows rebuild.

### First play test (user, 2026-11-07)

| Finding | Fix (`#UMBRA-43`) |
|---|---|
| The save stone was too short: the interaction ray (waist height) passed over it | Interaction now also casts a knee-height ray (0.3 m above the feet) when the waist ray finds nothing usable; save stones are taller (1.2 m). Tests: `PlayerControllerTests` (short object), `SettlementWalkTests` (save stone) |
| No feedback or animation; fighting among the dungeon's walls and other enemies looks odd | Every fight now happens in a separate battle arena scene, for both modes; the dungeon is kept aside and restored exactly. Whoever takes damage flashes red. Animation is left for a later phase (user's call) |

## 4. Play-through checklist (open, needs a person at the PC)

Start in the settlement (default). Saves go to `%APPDATA%\Umbra\UmbraEngine\saves`.

| # | Do | Expect | Result |
|---|---|---|---|
| 1 | Look at the top left | Level, HP, EN, XP; "Combat: turn-based [Tab] switch" | |
| 2 | E at Elder Mara, choose "I'll get it back." | Dialogue window; "Quest: The Stolen Relic - ..." appears | |
| 3 | E at the save stone (also from close up) | "Game saved" | |
| 4 | Walk through the portal to the dungeon | Dungeon loads; an autosave is written (no message unless it fails) | |
| 5 | Watch the entrance | Grey cubes (wolves) wander; they do not chase | |
| 6 | Walk into a wolf | The battle arena loads: you on one side, 1–2 wolves facing you; menu at the bottom, log at the top left | |
| 7 | Pick "Strike -> Wolf 1" until the fight ends | Hit fighters flash red; enemies act after a short pause; "Victory! +XP"; back in the dungeon at the same spot, that wolf gone, the others where they were | |
| 8 | Tab (outside a fight), then walk into a hall enemy | "Combat: real-time"; keys help at the bottom; 1 strikes, Q drinks a potion | |
| 9 | Press F | "You got away"; back in the dungeon; the enemy doesn't start a new fight for 3 s | |
| 10 | Try Tab during a fight | Nothing changes | |
| 11 | Defeat the bandit leader (purple), return to Mara, "I have the relic." | "Quest complete"; XP, skill point and iron sword | |
| 12 | Let an enemy kill you | "You died" menu; a failOn-death quest shows "Quest failed" | |
| 13 | "Load last autosave", then die again and "Load save" | Back at the dungeon entrance / the save stone, quest state as saved | |
| 14 | Esc while talking; Esc on the death menu | Walks away / nothing (no pause menu over death) | |
| 15 | Unplug the network and repeat 2–7 | Same behaviour (nothing needs the network) | |

## 5. Known limits

- Enemies are tinted cubes (placeholder art); the only hit feedback is a red flash.
- In a real-time fight, the player faces the way they walk, so they must walk toward an enemy
  to hit it.
- There is one arena for every area; a per-area arena field would be a small later addition.
- The hero unlocks skills by data only; there is no skill-tree screen yet (P7).
- The `Umbra` app part (roamers, `GameFlow`, HUD) has no automated test; its rules are covered
  through `GameSession` (`apps/umbra/GameFlow.md`).
