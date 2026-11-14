# The table

`LastCurtain.exe` draws the night as a table top: every shape but the pawns is one 1 m cube, scaled and coloured.

1. **Rooms**: a floor tile 2.2 m wide and 0.15 m thick per opened cell (cells are 2.4 m apart, `BoardView.hpp`). A lit room has its kind's colour; a known room in the dark is dark grey. A side a guest cannot pass (no door, a door facing a wall, a sealed passage: `CellView::walls`) gets a wall stub; a door to a room not yet opened gets a faint bar (the doorway).
2. **Pawns**: one figure per guest in a lit cell. All seven guests share one rigged model
   (`assets/models/guest.glb`, Hoang Gia Huy's) and every Taken shares another (`taken.glb`);
   each is tinted with its guest's `color` from `guests.csv`, lightened a quarter toward white
   with a faint glow so dark tints read in the dim room. The packed model is 2 units tall, feet at
   y = -1, facing -Z: the table draws it at `translate(at) * rotateY(yaw) * scale(s) * translate(0, 1, 0)`
   with s = 0.55 (guest, 1.1 m) or 0.65 (Taken, 1.3 m). A sliding pawn faces its step,
   `yaw = atan2(-step.x, -step.z)`, and its graph (`assets/animations/*.anim.json`) plays `walk`
   while `speed` is 1; otherwise `idle`. A guest who turns gets a new animator on the Taken model.
   If a model cannot be loaded the pawn is a box again, and the log says why. A gold ring marks the player, a green one an ally, a red one the Taken. Monsters are brown, the Taken's minions violet. Several pawns in a cell spread out by the golden angle (137.5 degrees), so none land on another.
3. **Walking**: a pawn slides from cell to cell in `Play::slideSeconds` (0.25 s), its position
   `from + (to - from) * s` with the same smoothstep `s = t*t*(3-2t)` as the camera, so each step
   starts and stops gently. While the player picks a square, its path shows as dots: gold for the
   steps the moves left can pay for, red after.
4. **Light**: indoors, a dim ambient (tinted red after the Turn) and one warm lamp 7 m over the middle of the table.
5. **Camera**: it looks at a target from 50 degrees above the table. `Aim` moves the target over some seconds with smoothstep easing, `t*t*(3-2t)`, so it starts and stops gently. The wheel changes the distance (5 to 22 m).
6. **Looking around**: dragging with the left button moves the table with the mouse: the look-at
   point shifts by the drag times `distance / 700` metres a pixel (x across, y toward the camera),
   so the board stays under the cursor at any zoom. A drag stops the camera following (`Play::Drag`)
   until the Focus button. A board click is the button let go without a drag.
7. **Labels**: "thinking..." and the group numbers float over pawns. `Above` is the pawn's top;
   `ScreenPoint` takes it through the camera's view and projection to clip space, divides by `w`
   (points behind the eye are dropped) and maps -1..1 to pixels, y down. The `labels` screen draws
   each at its point (`"at"`). Rings: gold for the player's group (1), teal (2), violet (3), red for
   the Taken.
8. **Picking**: the mouse position goes back through the projection and view (`inverse(projection * view)`) at the near and far planes. The line between those two points is the ray from the eye; `CellUnderRay` finds where it meets the table top, y = 0.
