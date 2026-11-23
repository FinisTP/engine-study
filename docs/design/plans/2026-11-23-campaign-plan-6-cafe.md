# Campaign plan 6: the café between fights — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The café as the campaign's home: a calm room (no shift, no customers' timer) where the party rests, talks, keeps items on the shelf, and where the player restores Tí's memory piece by piece; Lệ Thanh walks in and out with her story; the lines are authored and the local model may reword them when present.

**Architecture:** A `CafeVisit` in `umbra::campaign`: which talks are open (`talks/*.json`: who, conditions on facts and chapter, a scene id, facts set), the shelf and slot moves, Tí's restoring state. The app reuses the existing café scene's room and figures but runs a campaign mode with no shift. Talks are plan-3 scenes. The model path: when `models/vietnamese.gguf` (or the project's model) is present, `TalkMind` (plan 1 of the campaign, #324–#330) rewords an authored line, and the guard's 12 checks decide; any failure shows the authored line.

**Tech Stack:** C++20, Catch2; `campaign`, `talkmind`, `cafe` (the room), `ui`.

**Spec:** `docs/design/2026-11-23-campaign-slice-1.md` §7.7–7.9; `campaign-content-2026-11-22.md` (Tí, Bảo, Bà Mai, Lệ Thanh); the user's answers of 2026-11-23 evening: **freed Tí leaves the board at once (gold light) and waits in the café**; **café lines authored, the model optional** (rewords only when a model file is present and the guard passes).

## Global Constraints

- No timers, no customers, no money in the campaign café. The night simulation's café code is not changed; the campaign mode is a separate entry.
- Talks: each **once** unless marked `repeat`; at most **3 new talks a visit**, marked with a small lantern; old ones in a "Chuyện cũ" list.
- Restoring Tí: he arrives **blank** (one line: "…Tí? Tên em là Tí hả?"). Each visit after a won fight opens **two memory fragments** (a short interaction each: hand him an object from the shelf that matches the memory hint; right object → the fragment's scene; wrong → a gentle line, try again next visit). Six fragments in the slice; at **3** he gains *Đưa tin*, at **6** *Chân sáo* and his full voice. A fragment never locks the story.
- Lệ Thanh: appears after T1's rescue; walks between the counter, the door and the window; talks unlock by facts (`lethanh.first`, `t3.rite`, `stage1.money`). She never says her full story in the slice.
- Fight memories: a fight sets facts (`fell:<unit>`, `rite:<stage>`, `ratStole:<item>`), and talks can depend on them (Bà Mai teases the one who fell most; Bảo comments on the rite).
- Model: optional. Missing model → authored lines, no message. A model line is shown only after the guard passes; its result is **stored in the save** so a reload shows the same line.
- Commits from **#365**.

## Content (the slice's talks; drafted in Vietnamese for review)

| Talk | Who | Opens | Sets | Idea |
|---|---|---|---|---|
| `cafe-first` | Lệ Thanh | first visit | `cafe.known` | The café is "outside the house's rules"; the strings don't reach here. |
| `bao-1` | Bảo | first visit | — | Bảo's dry doctor humour; why he carries death papers. |
| `mai-1` | Bà Mai | first visit | — | Cold rice and old songs; she calls the player by name. |
| `bao-2` | Bảo | after T3 | — | If the player fell in T3: "Lần sau nằm xuống thì nằm chỗ có bóng mát." |
| `mai-2` | Bà Mai | after T3 | — | The rite: she is afraid of it. |
| `lethanh-rite` | Lệ Thanh | `t3.rite` | — | What the rite costs (hinted, not told). |
| `ti-arrives` | Tí | Stage 1 won | `ti.home` | Blank Tí; the first two fragments open. |
| `lethanh-money` | Lệ Thanh | `stage1.money` | — | The Rat's words about money; she turns away. |
| `ti-1`…`ti-6` | Tí | fragments | `ti.frag<n>` | The newspaper round, the slingshot, his sister, the tram bell, the rain on the tin roof, the name the Rat called him. Objects: a folded newspaper, a rubber band, a hair ribbon, a brass bell, a tin cup, a ticket stub. |

---

### Task 1: Talk files and the visit (#365)

`talks/*.json` loader (who, `opens` conditions: facts all/none, chapter ≥, `after` stage; `scene`; `sets`; `repeat`). `CafeVisit(save, talks) -> {newTalks ≤ 3, oldTalks}`. Tests: conditions; the cap of 3 (ordered by file order); once-only. Commit.

### Task 2: The shelf and slots (#366)

`MoveItem(save, from, to)` between a unit's 4 slots and the shelf; refuse to a full slot list; items stack only if the item def says so. Tests. Commit.

### Task 3: Restoring Tí (#367)

`fragments.json` (hint line, wanted item, scene, the ability unlocked at 3 and 6). `OfferItem(save, fragment, item) -> Right/Wrong`; two open after each won fight; the ability unlocks join Tí's kit. Tests: right and wrong; opening two per won fight; unlocks at 3 and 6; never more than 6. Commit.

### Task 4: The campaign café screen (#368)

The café room with the party's figures and Lệ Thanh walking (her path from the room's points), `cafe-campaign.ui.json` (talk list with lanterns, the shelf, Tí's fragments, "Ra bản đồ"), wired into `CampaignApp` in place of plan 5's stub. Tests: a capture; the flow test from plan 5 now visits the café. Commit.

### Task 5: The model, optional (#369)

`Reword(line, voice, situation) -> optional<string>` through `TalkMind` when a model file is present; guard failure or a timeout (from the options) → none. Stored in `save.lines[talkId:lineIndex]`. Tests with the stand-in model: a passing line shown and stored; a failing line falls back; a reload shows the stored line; no model → no call. Commit.

### Task 6: Content and records (#370)

The talks and fragments above, in Vietnamese with `{player}` and `{anh/chị}`; `docs/testing/s50-campaign-cafe.md` (§4: a visit on Windows with and without the model file; Tí's six fragments), ROADMAP, status. Ship.
