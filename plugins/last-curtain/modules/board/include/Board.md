# What the player sees (`board` module)

`MakeView(def, state, viewer)` turns the night into what the player's screen may show. The
program draws only this; nothing else of the state reaches the screen.

## The light rule

A cell is **lit** when any guest the viewer `Controls` (the player's guest and their allies),
standing and alive, lights it: `Lit(def, state, theirCell, cell, Stat(Light))`, steps through
passages. An opened cell that no one lights is **known**: its shape and kind show, not who is in
it. Pawns, monsters, minions and the Taken appear only in lit cells; the viewer's own pawn always.
A dead viewer, or viewer -1, sees everything (watching the bots finish the night).

## Walls and doorways

Each side of an opened cell is one of three: a **passage** (`Passage`: both rooms opened with
doors facing, not sealed), a **doorway** (`Doorway`: a door to a cell not opened yet, while rooms
remain) or a **wall** (everything else, `CellView::walls`): a door that faces another room's
wall is a wall, so the table never shows a gap a guest cannot pass.

## Paths

`PathTo(def, state, guest, cell)` is what a click on a square walks: a breadth-first search from
the guest's cell through passages (cells barred this turn left out), so the first time it reaches
the square is a shortest walk. Each cell remembers the cell it was reached from; following those
back from the square and turning the list round gives the steps in order. An unopened square is
reached through a doorway of a cell on the way (the last step opens it); an opened square must be
lit for the guest.

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

## Narration

`Narrate` turns one applied action into a `Beat`: a camera cell, the pawn's walk and two lines of text.

1. The beat is **seen** when the actor's start cell was lit for the viewer, or its end cell or a newly opened room is lit now.
2. An unseen beat has no caption, path or camera: the opened room still appears, because the board draws the state.
3. The **caption** is the decision (`captions.json` keys `move`, `open`, `fight` ...); the **result** comes from the log entries the action wrote (`check`, `found`, `down`, `died`, `hit`, `killed`), joined with "; ".

The lines are plain templates for now; a later plan lets the model word them.
