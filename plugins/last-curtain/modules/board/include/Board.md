# What the player sees (`board` module)

`MakeView(def, state, viewer)` turns the night into what the player's screen may show. The
program draws only this; nothing else of the state reaches the screen.

## The light rule

A cell is **lit** when any guest the viewer `Controls` (the player's guest and their allies),
standing and alive, lights it: `Lit(def, state, theirCell, cell, Stat(Light))`, steps through
passages. An opened cell that no one lights is **known**: its shape and kind show, not who is in
it. Pawns, monsters, minions and the Taken appear only in lit cells; the viewer's own pawn always.
A dead viewer, or viewer -1, sees everything (watching the bots finish the night).

## Steps

On a turn the viewer plays (their guest or an ally, not in battle), every legal `Move` through a
door (not the transport move) marks the cell it leads into with its index in `LegalActions`: the
numbered badges, and what a click there applies.

## Coordinates

Cells are `cellSize` (2.4 m) apart on the table plane (y = 0). The board is centred on the origin:

```
x = (column - (width  - 1) / 2) * cellSize
z = (row    - (height - 1) / 2) * cellSize      row 0 is at -z (far side), the foyer near the camera
```

`CellAtPoint` inverts it: `column = floor(x / cellSize + width / 2)`, the same for rows.
