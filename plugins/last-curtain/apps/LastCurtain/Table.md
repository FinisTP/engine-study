# The table

`LastCurtain.exe` draws the night as a table top: every shape is one 1 m cube, scaled and coloured.

1. **Rooms**: a floor tile 2.2 m wide and 0.15 m thick per opened cell (cells are 2.4 m apart, `BoardView.hpp`). A lit room has its kind's colour; a known room in the dark is dark grey. A side a guest cannot pass (no door, a door facing a wall, a sealed passage: `CellView::walls`) gets a wall stub; a door to a room not yet opened gets a faint bar (the doorway).
2. **Pawns**: one box per guest in a lit cell, in the guest's colour. A gold ring marks the player, a green one an ally, a red one the Taken. Monsters are brown, the Taken's minions violet. Several pawns in a cell spread out by the golden angle (137.5 degrees), so none land on another.
3. **Light**: indoors, a dim ambient (tinted red after the Turn) and one warm lamp 7 m over the middle of the table.
4. **Camera**: it looks at a target from 50 degrees above the table. `Aim` moves the target over some seconds with smoothstep easing, `t*t*(3-2t)`, so it starts and stops gently. The wheel changes the distance (5 to 22 m).
5. **Picking**: the mouse position goes back through the projection and view (`inverse(projection * view)`) at the near and far planes. The line between those two points is the ray from the eye; `CellUnderRay` finds where it meets the table top, y = 0.
