# The Last Curtain, plan 4: tiles, group battles, ghosts, the café

The user's notes of 2026-11-20 evening (19 notes, then the café), with their answers:
walls become tiles (rebuild), ranges by a column (walk or straight), two of one side per tile,
group battles where everyone picks in speed order, ten ghosts as art and data, a café hub walked
freely, a saved career, a timed serving rush with short recipes at stations. The feel: a swap
between a cosy café and hard battles, like Dave the Diver.

## 1. Rules (done first, each with tests, then the soak)

| # | What | Where |
|---|---|---|
| R1 | A heal never goes to someone at full HP (`FullHp`) | `Abilities.cpp` |
| R2 | `reach` column: `walk` (steps) or `straight` (Manhattan, walls ignored) | `abilities.csv`, `Battle.cpp` |
| R3 | The board is tiles: square (x, y) is tile (2x, 2y); a doorway or a wall between; corners open only inside open blocks. Gameplay distances doubled in data; `board.stride` 2 for monsters' steps | `Board.cpp` `Expand` |
| R4 | `squareCapacity` 2, never with a foe; the stage holds one | `Room` in `Battle.cpp` |
| R5 | Group battles: an attack takes in everyone of each side on the two tiles (up to two a side). Each round every living fighter, in speed order (first strike first, attackers first on a tie), picks a battle ability and a target in the other group; a shield is a fighter's own. Ends after `battle.rounds` or when a group is down | `Battle.cpp`, `MonsterMind.cpp`, `MatchBots.cpp` |

Saves become version 3 (the tile board, shared tiles, group battles).

## 2. The match's screens (the 19 notes)

- Battle pacing: every blow told one beat at a time, the player's too; HP on the cards follows what
  has been told; a figure that is downed stays standing until its fall is told, then falls.
  No round banner and no next actor while a fight is staged; the round counter never shows past
  the last round. The picking fighter: a bobbing arrow on the stage, its card lifted.
- Board: labels and arrow above the figure's own head (two on a tile stand apart), the camera
  follows tightly, reach and targets as blinking half-transparent fills, the actions panel beside
  the actor, the hint line always says what to do next.
- Screens: buttons with hover, press bounce and selected states (engine UI); the pick screen's
  chosen cards lifted and lit; the order strip twelve long, hover shows the forecast, click
  centres the camera; modal screens take the mouse (the board under them gets nothing); level
  cards describe what an ability does; essence big and gold.

## 3. Ten ghosts (art + data)

One ComfyUI sprite per monster kind (the shipped five redrawn) and five new kinds to reach ten,
each a `monsters.csv` row with stats, abilities, spawn rows and a café order. Sprites as the
characters': full figure, plain background, cut out, `art/sprites/<id>.png`.

## 4. The café hub and the career

- **Flow**: start → the café (hub) → the door: pick three, next match → the match → back to the
  café with the takings (essence won becomes money) and the levels kept.
- **Career save** (`career.json`, versioned): money, day, each character's level, XP, learned
  abilities and boosts, unlocked flags. A match starts its characters from the career; the
  match's end writes back (levels, abilities, boosts; money from score).
- **The room**: a small 3D café (counter, three stations, tables, the door, a notice board), a
  painted backdrop; the chosen character walks freely (WASD or click), E at things.
- **Open / close the shop** at the counter's sign. Open: a shift of `shiftSeconds`; ghosts come in
  by a schedule, sit, show an order bubble and a patience bar. Recipes are 1-3 steps at three
  stations (grinder, phin, milk & ice). Hold a cup, add the steps in order at the stations, carry
  it to the ghost. Right and quick pays more and tips; wrong or late leaves. The day's takings
  screen at the end.
- **Conversations**: a served ghost and the serving character talk (two or three lines from
  `cafe-talk.json`, by ghost kind and character), backstory a little at a time, each line once.
- **Rest / tend**: the notice board shows each character (portrait, level, abilities); resting
  restores nothing yet (matches start whole) but is where shop, upgrades and unlocks will go.
- Headless logic (`Cafe` module: shift clock, queue, orders, steps, pay) is tested like
  `MatchPlay`; the room draws and walks in the app.

## Order

R5 → the 19 notes' screens → ghosts → the café logic → the café room → the career and flow →
record and ship. Each part ships when its tests pass.
