# Campaign plan 6: the café hub, brewing memories, and the notebook — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The calm café as the campaign's hub: walk, talk, and brew. Memory drinks bring back a
recruit's memories and unlock an upgrade version; general drinks give capped boosts; ingredients and
keepsakes wait in the café's storage. The notebook (Sổ tay) shows characters, ingredients and keepsakes,
and sets up the party outside battle. Tí joins through memory 1 and recovers memories 2–3 in the slice.

**Architecture:** `umbra::campaign` gains `Recipes` (`recipes.json`), `Memories` (per recruit:
`memories.json`), `Brew(save, recipe, recruit) -> result`, `Boost(save, drink, character)`, `Talks`
(`talks/*.json`), all pure functions on the campaign save (plan 7 writes it; until then a test save).
The app runs a **campaign café mode** with no shift: the room, the walker and figures from today's café,
new screens `cafe-hub`, `brew`, `notebook`. The career mode's café is untouched.

**Spec:** `docs/design/ux/cafe.md`, `ux/notebook.md`, `ui-handoff` §2 rows 4–9, `ti-content` §2–7; the
user: the recruits stay ten; general drinks capped at **3 per stat per character**; Tí joins after
memory 1, a talk with no item.

## Global Constraints

- Nothing in the café HUD counts time or money. No wrong answers in talks; nothing in the café can fail.
- **Memory drinks:** what you serve chooses which memory comes back. Each memory's scene ends with the
  player's answer, which **chooses the upgrade version** (two per upgrade; final, confirmed, P8).
- **General drinks:** one per pick, applied to one character; `options.csv` `boostCapPerStat,3`; the cap
  counts drinks (Nước gừng: +2 max HP each, so +6 at most).
- Ingredients and keepsakes live only in storage; café-only items never enter the 4 battle slots.
- The notebook: tabs Nhân vật, Nguyên liệu, Kỷ vật (Q/E); in battle read-only with the fight paused;
  outside battle the 4 slots and "Ra trận" are edited there. Unknown entries show "?".
- The local model may reword an authored talk line only when a model file is present and the guard
  passes (plan 1's TalkMind); the result is stored in the save.
- Tí's lines and the memories as `ti-content` §4–6, with the user's corrections when they come.
- Commits from **#372**.

## Content (slice 1)

| What | Source | Notes |
|---|---|---|
| Tí memory 1 *Tên mẹ* | talk with Lệ Thanh after Stage 1, no item | Tí joins; choice sets `ti_hope` / `ti_fed` |
| Tí memory 2 *Lời hứa Tết* | Mứt gừng (Gừng + Đường) | choice → Đưa tin Về kịp / Phần quà |
| Tí memory 3 *Lời nhắn* | the keepsake Mẩu giấy gấp tư | choice → Ná cao su Báo động / Bắn trước |
| Tí memory 4 *Bãi cát trắng* | Nước dừa (Stage 2) | data only; locked in the slice |
| Recipes | Lệ Thanh teaches Mứt gừng and Nước gừng after Stage 1 | |
| Nước gừng | Gừng ×1 | +2 max HP, anyone, cap 3 |
| Talks | Lệ Thanh (first visit, after T3: the rite, after Stage 1: money), Bảo, Bà Mai (after T1, after T3, fight memories: who fell, the rite) | fact-gated, ≤ 3 new a visit |
| Tí idle lines | `ti-content` §4.6 | one per visit, in order |

---

### Task 1: Storage, item flags and recipes (#372)

Save fields used: storage (ingredients, keepsakes, items), recipes known. `recipes.json` (id, makes,
needs, kind memory/general, for whom). `CanBrew`, `Brew` (spends, records). Tests: missing ingredients
say what and where (from stage drop data, "?" when unknown); a café-only item can't enter a slot. Commit.

### Task 2: Memories and upgrade versions (#373)

`memories.json` per recruit (memory id, source talk/recipe/keepsake, scene id, the upgrade and which
choice gives which version). After a memory scene, the choice's fact picks the version; the confirm is
the app's. Tests: memory 1 joins Tí; memory 2 with each answer gives each version; a version is final;
memory 4 locked. Commit.

### Task 3: General drinks and the cap (#374)

`Boost(save, drink, character)`: refused at the cap; tests: 3 then refused; the cap from `options.csv`.
Commit.

### Task 4: Talks (#375)

`talks/*.json` loader (who, opens: facts all/none, chapter, after stage; scene; sets; repeat), ≤ 3 new a
visit; the stage result's facts (`fell:<unit>`, `rite:<stage>`, `ratStole:<item>`). Tests. Commit.

### Task 5: The campaign café screen (#376)

`cafe-hub.ui.json`: the room, name plates within 3 m (gold line "Có thể pha chế" when a brew waits), the
narration line, one prompt (E), the door (to the world map; plan 7), Tab for the notebook. `brew.ui.json`:
recipes column (memory drinks first, then general), the preview (a hint line, the two versions side by
side, ingredients), brew → the memory scene → choose the version (confirm) → toast. Tests on the data;
captures. Commit.

### Task 6: The notebook (#377)

`notebook.ui.json` with three tabs, the list, the detail, the side panel; read-only in battle (wired into
plan 5's stub, the fight paused); slots and Ra trận editable outside. Tests: read-only refuses edits;
reopens on the last tab and entry; unknowns are "?". Capture both text sizes. Commit.

### Task 7: The model, optional (#378)

`Reword(line, voice, situation)` through TalkMind when a model file is present; guard or timeout → the
authored line; stored in the save. Tests with the stand-in model. Commit.

### Task 8: Content and records (#379)

Tí's memory scenes 1–4, idle lines, talks, recipes and descriptions in Vietnamese (from `ti-content`, with
the user's corrections); `docs/testing/s50-cafe-hub.md` (§4: a visit on Windows, brew Mứt gừng, pick a
version, a boost to 3/3, the notebook in and out of battle). ROADMAP, status. Ship.
