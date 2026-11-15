# The Last Curtain: the third round of feedback (17 items) — verification record

## Record

- Date: 2026-11-15
- Commits: `issue #UMBRA-192` … `#UMBRA-199`.
- Tester: Claude (remote session), Linux cloud workspace; pictures with SwiftShader and `ui-preview`.
- State: **implemented**; the Windows play check (§4) is open.
- The user's feedback (18:07), 17 items: Look not centred on the player's room; a zoom into the
  room; replies in chats; room view at the start of the turn; room view as a play mode (click
  rooms, items, figures; all lit rooms); mouse-first input; drag and zoom in room view, Look as a
  switch; the camera always on the acting character; a preview of bot deeds; effects announced;
  room markers; a new move rule; event cards not covering the room; the Taken's name; visible bot
  fights; readable walls under the mood; the top bar on bots' turns.
- User decisions: moves count as the shortest way back to where the turn began, committing at a
  new room or a stopping room (bots too); the top bar shows the acting bot, marked "THEIR TURN",
  with the player's HP in a chip.

## What was built

| Items | Where |
|---|---|
| 4: a bot's last waiting beat that handed the turn to the player kept the room view with its caption hidden; now the caption shows first | `#UMBRA-192`, T153 |
| 8: the camera follows whoever acts (bots in sight too); a drag frees it until the next turn | `#UMBRA-193`, T154 |
| 17, 14, 10: the acting guest in the top bar; the Taken by its risen name everywhere (`ShownName`); bonus rooms announced on entering and when paid | `#UMBRA-194`, T155, T156 |
| 13: event, dice, find and chat cards at the right, the battle panel at the bottom, no dimming | `#UMBRA-195` |
| 12: moves = budget less the shortest way back to the origin (`Distance`); a new room or a stopping room commits; a step cap (3 × Speed, `rules.json` `moves`); save version 4; three Taken forms rebalanced | `#UMBRA-196`, T157 |
| 1, 2, 5, 6, 7: the room view is a play mode: every opened room at full size where its tile lies, a zoom between the views, clicks on rooms (walk), figures (chat or fight), monster cards (fight) and floor items (take); drag and wheel; Look became the view switch; a waiting caption goes on with any click | `#UMBRA-197`, T158 (`Table.md` "Room view") |
| 9, 15, 3: a bot's deed in sight is previewed (tried on a copy of the night) before it happens; blows say who hit whom, the damage and HP left, with the damage over the struck guest; used items named; chat answers get the asker's reply (written, reworded by the model) | `#UMBRA-198`, T159-T161 (`Play.md`) |
| 11, 16: markers over rooms with an item, event, trap or bonus (shape and colour); walls, figures and items glow with the darkening mood; pale table walls | `#UMBRA-199`, T162 |

## 1. Automated checks

| Check | Result |
|---|---|
| T153-T162 and the UI click test (each written first and seen failing) | pass |
| Night suite: 214 cases | pass (GCC) |
| Soak, 1,000 nights on seeds 1, 20001, 80001, after the move rule: every Taken form 41-59 %, every goal in band | holds (The Drowned Physician HP 42 → 38, The Folded Girl 34 → 38, The Iron Debtor 53 → 51) |
| Full CTest under GCC, Clang, ASan + UBSan | 55/55 each (§1b) |

## 1b. Full runs

`/home/claude/verify.sh` (every CTest, Debug) at `#UMBRA-199`: GCC 55/55, Clang 55/55, ASan + UBSan
55/55 with no sanitizer report.

## 2. Pictures

`--look` on seed 7 as Huy: the room view with every opened room, figures in their rooms, the
player's ring and the pale plates of the rooms one step away. The table: pale walls under the
evening. A bot's preview: "Le Thanh opens a door: the Rotten boards", the event named with its
story, Continue. Not caught in a still: the zoom between views, markers over armed rooms, the
late-night readability.

## 4. The user's check on Windows

1. Switch views with Room view / Board view: the camera zooms in and out; play a whole turn in
   the room view (walk by clicking rooms, take an item, talk to someone in your room).
2. Drag and wheel in both views; Focus brings the camera back; on bots' turns the camera follows
   them.
3. Walk out and back: your moves come back; open a door: the moves left stay as they were.
4. A bot walks into a trap in your light: first the preview (the room, the event), then the
   result. A bot's fight: each blow says who hit whom and the HP left, with the damage over them.
5. Answer a chat: the asker replies.
6. Markers over item, event, trap and bonus rooms; walls readable at 23:00 and after the Turn.
7. The top bar on a bot's turn shows the bot, THEIR TURN and your HP.

## 5. Follow-up (19:44): three more items

| Item | Where |
|---|---|
| A drag in the room view blanked the screen for good: an Aim taken there read the camera's scaled room-space target as a table one, so every later Aim flew further off. The camera's math is now `TableCamera` (its target always on the table), tested | `#UMBRA-201`, T163 |
| The Focus button is gone: a click on the top bar's portrait (a list of one with the action `focus`) brings the camera back to whoever acts | `#UMBRA-201`, T128, T144 |
| The view switch works only on the player's turn or its rest (`Play::CanView`); the HUD button hides otherwise | `#UMBRA-201`, T144 |

Checks: the night suite 216 cases under GCC and ASan; Clang full CTest 55/55. The new camera tests
passed at once, being written with the fix (the old camera lived in `Table`, which needs a GPU);
the drag is not checked on a GPU here.
