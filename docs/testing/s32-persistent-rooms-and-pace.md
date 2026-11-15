# The Last Curtain: persistent events, the turn arrow, the room panel and the pace — verification record

## Record

- Date: 2026-11-15
- Commits: `issue #UMBRA-178` (persistent events and traps), `#UMBRA-179` (arrow, room panel, tap to
  continue, Skip, moments at full size, `options.csv`).
- Tester: Claude (remote session), Linux cloud workspace; pictures with SwiftShader and `ui-preview`.
- State: **implemented**; the Windows play check (§4) is open.
- The user's feedback (2026-11-15), eight items: a turn indicator; visible room effects; a room
  panel; persistent events and traps with clear conditions; Skip; bot beats that wait for a tap;
  an options file; the room at full size for what happens in sight.
- User decisions: a met clear condition clears the event or trap for everyone; all traps persist,
  plus events marked `persist`; tap to continue only on beats where something happens; full size
  for items picked up, events and traps, chats, deaths and fights.

## What was built

| Item | Where |
|---|---|
| 4. Persistent events and traps: `persist`, a clear condition (`clear` stat N+ on entering, or carry an item), cleared by a passed roll or the condition, for everyone; save version 3; soak re-balanced | `#UMBRA-178`, `events.csv`, `Night.*` |
| 1. A gold arrow bobs over whoever's turn it is, on the table and the stage | `#UMBRA-179`, `Table::SetActing` |
| 3. The room panel (bottom left): the room, what waits there and how to clear it, items on the floor, who else is there | `Play` `here` data, `hud` screen |
| 5. Skip (bot turns): no taps, pauses or line waits until the player's turn | `Play::HandleUi("hud","skip")` |
| 6. Tap to continue: a seen bot beat where something happens waits for Continue (or Space) | `Play` `waitTap_`, `caption` screen |
| 7. `assets/options.csv`: beat, slide, think and line times; tap to continue; moments; camera distance, zoom range, tilt and ease | `Options.*` |
| 2, 8. Moments: such a beat (and the player's own event or roll) shows its room at full size; clips for an item (reach), a roll, a trap that hurts (hit), a chat (both talk), a clearing (attack) | `Play::StageCell`, `Table::React` |

## 1. Automated checks

| Check | Result |
|---|---|
| T146: persistent events stay armed, clear by roll or condition for everyone, survive a save | pass (RED first) |
| T147: `options.csv` loads; an unknown key, a word or a negative value is refused with its row | pass (RED first) |
| T148: a bot beat where something happens waits for a tap and is staged; Skip runs to the player's turn with no taps | pass (RED first) |
| T149: the room panel names the room and the armed event with its clear condition | pass (RED first) |
| Soak, 1,000 seeds on three ranges: every goal 45-65% | pass |

## 2. Pictures

`LastCurtain --seed 7 --player huy --capture x.png --frames 60` (SwiftShader): the arrow over the
player's pawn and the room panel ("Stage armoury / Quiet now."). `ui-preview caption` with a waiting
beat: the Continue button. A waiting beat on the stage was not caught in a still (it needs a bot
turn and input).

## 4. The user's check on Windows

1. The arrow follows the turn, over the pawn on the table and the figure on the stage.
2. A bot picks up an item, sets off a trap or chats in your light: the room opens at full size, the
   figure moves, and the caption waits for Continue. A beat where nothing happens does not wait.
3. Skip runs to your turn at once.
4. The room panel tells what waits and how to clear it; walking in with the condition clears it.
5. Change `options.csv` (say `beatSeconds` and `cameraTilt`), restart: the game follows it.
