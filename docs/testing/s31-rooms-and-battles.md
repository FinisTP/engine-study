# The Last Curtain, plans 4b and 4c: a room at full size and battles in it — verification record

## Record

- Date: 2026-11-15
- Commits: `issue #UMBRA-174`, `#UMBRA-175`, `#UMBRA-177` (records). Design
  `docs/design/2026-11-15-art-rooms-battles.md`; plan `docs/design/plans/2026-11-15-rooms-and-battles-plan-4bc.md`.
- Tester: Claude (remote session), Linux cloud workspace; captures with SwiftShader. No person has
  walked a room or watched a battle yet.
- State: **implemented**; the Windows play check (§4) is open.
- User decisions (2026-11-15): a fixed corner camera, like a stage; a room opens at full size only
  for Look; battles the player fights, and bots' battles in the player's light, play at full size.

## What was built

| Part | Where |
|---|---|
| Look in the turn menu; the night waits; Back; a doorway is the board's Move; what the stage shows (the room, or a battle in light, with its sides) | `#UMBRA-174`, `Play.*`, `look` screen |
| The stage drawn: the room's painting as an 8 m floor, back walls and low front walls with door gaps, the fixed corner camera, figures at full size, monster cards, a battle's two sides; walking by clicking the floor; clips from the log (attack, hit, power, lying down) | `#UMBRA-175`, `Table.*` ("The stage" in `Table.md`), `Main.cpp`, `--look` |

## 1. Automated checks (T144)

| Check | Result |
|---|---|
| Look opens only on the player's menu; the night waits; Back returns | pass (RED first) |
| A doorway with a legal Move leaves the look and moves; a barred side does nothing | pass (RED first) |
| A battle in the player's light is staged with both sides; one in the dark is not | pass (RED first) |

## 2. Pictures (SwiftShader)

`LastCurtain --seed 7 --player huy --look --capture x.png --frames 40`: the room with door gaps and
the player's figure at full size; a battle layout (with a temporary patch adding a monster and a
Taken) shows the sides facing each other. The clips themselves were not caught in a still.

## 4. The user's check on Windows

1. On your turn choose Look around: your room at full size from a corner; click the floor and your
   figure walks there; walk into a doorway and the move is taken on the table.
2. Attack a monster: the fight plays in the room, blows play attack and hit, a fallen guest lies
   down; the battle menu works as before.
3. A bot's battle in your light also plays in its room; one out of sight stays on the table.
4. Is the camera corner right? Should front walls be lower or gone?
