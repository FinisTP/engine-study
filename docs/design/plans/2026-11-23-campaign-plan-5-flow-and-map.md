# Campaign plan 5: the campaign's flow, save and the theatre map — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One campaign you can play from the title to the end of Stage 1 and keep: a title screen with the player's name and form of address, a save, the painted theatre map you walk room to room (story rooms, skirmish rooms that refill, the café), progress and XP, items and the four slots, game over, and the language option.

**Architecture:** `umbra::campaign` gains `CampaignSave` (all changing data, version 2, written atomically) and `TheatreMap` (`map.json`: rooms, corridors, unlock rules). Pure functions advance the save (`WinStage`, `LoseStage`, `Unlocked`, `RefillSkirmishes`, `XpFor`). The app's `CampaignApp` is the one flow: title → map → scene/fight/café → map. It owns no rules: it calls `BuildStage` (plan 4), `ScenePlayer` (plan 3) and the café (plan 6).

**Tech Stack:** C++20, Catch2, nlohmann/json; `campaign`, `match`, `ui`; the app.

**Spec:** `docs/design/2026-11-23-campaign-slice-1.md` §7.1–7.3, §7.6–7.10; the user's answers of 2026-11-23 evening: **a painted map screen** (the dark oil-pastel plan of the opera house; rooms as lit or dark spots joined by corridors; a figure walking room to room); **skirmish XP only up to the next story stage's recommended level**, items still given; **skirmish rooms refill after each story stage won, each time with a new twist**.

## Global Constraints

- Save version 2 (version 1 was the slice handoff's sketch; refuse anything else with a clear message, keep the old file). Written to a temp file then renamed; a corrupt save shows "Không đọc được bản lưu" and offers a new campaign without deleting the file.
- The title asks the player's **name** (text field, 1–16 letters, Vietnamese letters allowed) and **form of address** (`anh` / `chị`); both fill `{player}` and `{anh/chị}`.
- The rite: once unlocked, using it is **saved at once** (no reload to undo it).
- Game over: **Thử lại** (retry the stage from its start), **Về quán** (back to the café; the map keeps the stage open), **Thoát** (to title). In T1 only Thử lại.
- Items: **4 slots** a unit; extras in the café's shelf (plan 6); map items taken **once per refill**.
- XP: a stage's `xp` on a win; skirmish XP **stops at the next story stage's recommended level**; a party member levels at fixed thresholds from `growth.csv`.
- No timer pressure on the map; walking is a short slide between rooms (≤ 0.6 s a corridor, skippable).
- Language: `vi` default, `en` from the pause menu, saved in prefs (not the campaign save).
- Commits from **#358**.

## The theatre map (content)

The opera house plan seen from above, painted dark, rooms lit as they open. Twelve rooms in the slice's file; the slice uses six, the others are dark shapes with names hidden ("?"), so the player sees how big the house is.

| Room | Kind | Opens when | Notes |
|---|---|---|---|
| Sảnh chính (foyer) | story T1 | start | Where the figure first stands. After T1: a quiet room (re-enter: a short scene that changes per chapter). |
| Quán cà phê (the café, outside the side door) | café | after T1 | Home: talks, shelf, Tí's restoring (plan 6). |
| Hành lang chân dung | story T3 | after the café's first visit | |
| Kho đạo cụ | skirmish | after T3 | Twists by chapter (plan 4). |
| Cầu thang phụ | skirmish | after T3 | |
| Phòng vé | story Stage 1 | after T3 | Recommended level 3. |
| Phòng hóa trang, Hầm sân khấu, Ban công, Phòng nhạc, Gác thượng, Sân khấu | later | locked | Dark, "?"; each with a lock line that hints at what's inside ("Có tiếng hát vọng ra từ dưới sàn."). |

Room states: **dark** (locked), **lit** (open, not done), **done** (a small gold mark), **refilled** (a new twist: a faint red glow and the twist's name). Story rooms already won can be re-entered to replay their scene only (no XP).

The figure: the player's form-coloured silhouette; walking follows corridors (the shortest path on `map.json`'s graph). Selecting a room shows its card: name, kind, recommended level vs the party's, rewards ("Kinh nghiệm" or "Đã đủ kinh nghiệm cho chặng sau", items left), the twist if refilled.

---

### Task 1: The save (#358)

`CampaignSave` (`version`, `seed`, `player{name, form}`, `party[]{id, level, xp, hp, items[4]}`, `shelf[]`, `rooms{id → state, refill, itemsTaken[]}`, `chapter`, `facts[]`, `riteUnlocked`, `riteUsedIn[]`, `tiFragments`, `here` (the room the figure stands in)). `SaveCampaign(path, save)` (temp + rename) and `LoadCampaign(path) -> optional<CampaignSave> + error`. Tests: round trip; a wrong version refused; a corrupt file refused and left on disk; an interrupted write (temp left) does not replace the good file. Commit.

### Task 2: The map's file and rules (#359)

`assets/campaign/map.json` (rooms with `id`, names vi/en, `kind`, `stage` or `skirmish` id, `at` (x,y on the painting, 0–1), `opensAfter` (a fact or room id), `lockLine`; corridors as pairs). `LoadTheatreMap` validates (unknown stage, unreachable room, a corridor to nowhere). `Unlocked(map, save) -> set`, `PathBetween(map, a, b) -> rooms`. Tests: the slice's openings in order; a path along corridors; validation errors name file and field. Commit.

### Task 3: Progress (#360)

`WinStage(save, stage, result)` (XP with the skirmish cap, the fight's HP kept or restored per stage file, items picked up go to slots then shelf, the room marked done, facts set, chapter advanced on a story win), `LoseStage`, `RefillSkirmishes(save, map)` (after each story win: each skirmish room's `refill`+1 picks its next twist from its stage file; taken items come back), `XpFor(save, stage)` (0 once at the next story stage's level). Tests: a skirmish gives XP up to level 3 before Stage 1 and 0 after; a refill after Stage 1 changes the twist and restores items; the rite use is saved at once. Commit.

### Task 4: The title (#361)

`title.ui.json` (Tiếp tục / Chơi mới / Tùy chọn / Thoát; Tiếp tục hidden without a save), `newgame.ui.json` (name field, anh/chị switch, a sample line that fills in as you type: "Chào {anh/chị} {player}."). The engine UI gains a text field if it has none (check `engine/ui` first; else a small one: typing, backspace, Vietnamese input through SDL text events). Tests: the name rule; the sample line; a `ui-preview` capture. Commit.

### Task 5: The map screen (#362)

`map.ui.json` (the painting, room spots placed from `at`, the figure, the room card), `MapView` (states drawn as tints and marks; the figure's slide along `PathBetween`; keyboard and mouse: arrows move between neighbouring rooms, Enter opens the card). Until the painting exists, a placeholder: dark paper with lines from the corridors. An art request for the painting goes in `docs/design/art-requests.md` (dark oil pastel, top-down opera-house plan, no text, 1920×1080, rooms as soft empty spots). Tests: the card's reward line; the figure's path; the capture. Commit.

### Task 6: The one flow (#363)

`CampaignApp`: title → (new: the prologue scene) → map → room: story (scene `start`, fight, scene `end`, back to map), skirmish (fight), café (plan 6; until then a stub that says so), game over screen (`gameover.ui.json`), the pause menu's language switch. Developer flags: `--stage <id>`, `--save <file>`, and **mid-stage save for developers only** (`--dev`), not offered to players. Tests: a scripted walk through the slice with stand-in fights (the flow test wins each stage by setting the result) ends with Tí in the party, the skirmishes refilled and the save written. Commit.

### Task 7: Records (#364)

`docs/testing/s49-campaign-flow.md` (§4 on Windows: a new campaign with a Vietnamese name, walk the map, win T1, quit, continue, lose a fight and see each game-over choice, a skirmish after the cap gives no XP, the refill after Stage 1), ROADMAP, status, CLAUDE.md. Ship.
