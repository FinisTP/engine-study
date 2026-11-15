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
   until the next turn or a click on the top bar's portrait. A board click is the button let go without a drag.
7. **Labels**: each figure's portrait and name float just over its arrow; every room in light has its
   name at its near edge (`RoomLabel`); "thinking..." and the group numbers float over pawns. The
   labels sit under the HUD's panels. `Above` is the pawn's top;
   `ScreenPoint` takes it through the camera's view and projection to clip space, divides by `w`
   (points behind the eye are dropped) and maps -1..1 to pixels, y down. The `labels` screen draws
   each at its point (`"at"`). Rings: gold for the player's group (1), teal (2), violet (3), red for
   the Taken.
8. **Picking**: the mouse position goes back through the projection and view (`inverse(projection * view)`) at the near and far planes. The line between those two points is the ray from the eye; `CellUnderRay` finds where it meets the table top, y = 0.

## Room view (plans 4b and 4c, the user's feedback of 2026-11-15)

On their own turn (or its rest) the player switches between the table and the room view (`Room
view` / `Board view` in the turn menu, or the HUD button; `Play::RoomView`, `CanView`); a moment (an event, a deed in sight, a
battle in light, `Play::StageCell`) shows the room view on its own. The night goes on in either.

1. **The world:** every opened room at full size, 8 × 8 m, where its tile lies on the table: a
   cell's centre times `roomScale` (8 m over the 2.4 m between cells). The floor is the room's
   painting; walls are 1 m on every side, 0.1 m inside the room, with a 1.8 m gap where a guest can
   pass (a passage or a doorway); featured rooms have gold walls; the player's goal rooms glow,
   pulsing, on the table and in the room view. Out of the player's light a room is drawn at
   35 % brightness. Guests stand where their pawns stand (`Place` × `roomScale`, so a slide walks
   them from room to room); monsters are framed cards at the back of their room, floor items
   painted tokens at the front. A battle in sight stands its sides apart in its room.
2. **The camera** (`TableCamera`): the target always stays on the table (an Aim from the room view
   once took the scaled target for a table one and flew off: blank screen); the room view scales it,
   seen from the front corner
   (normalize(5.2, 5.1, 7.5) × the room distance). Switching eases `roomT_` 0 → 1 over 0.6 s; on
   the way the camera glides from the table's seat (scaled, so the rooms first appear exactly
   where the tiles were) to the corner: a zoom-in. Drag pans (the same pixels move the table
   `roomScale` times less); the wheel changes the room distance (6-45 m).
3. **Clicks:** `PickThing` checks the things drawn last frame (figures at knee height, items on
   the floor); a figure engages (chat, or a fight after midnight), a monster card is fought, an
   item is picked up. Otherwise `Pick` (the ray scaled back to the table) gives the room: a walk
   there, as on the table. Rooms one step away have a pale plate; a walk shows gold and red discs.
4. **Clips:** `React` reads each frame's new log entries: a hit plays `attack` on the attacker and
   `hit` on the target; a power, a cleared event or a goal met plays `attack`; picking up an item or
   a roll plays `talk` (a reach); a trap or event that hurts plays `hit`; a chat plays `talk` on
   both; a guest down or dead lies down (`dead`) until woken. A gold arrow bobs over whoever acts.

## Markers and readability

A lit room that holds something for the player has a marker floating over it (`BoardView`
`CellView::mark`), each its own shape as well as colour: an item room still holding its item, a
gold tablet; an event (armed, or a room still holding one), a violet diamond; a trap, a red spike;
a bonus room the player has not had, a green plus. On the table at 1 m, in the room view over the
room's back corner. Walls, figures, cards and item tokens glow by `Readability(mood)` (0.2 in the
evening, up to 0.7 in the hunt), so they stay readable under the dark and red grade; walls on the
table are pale.

`--look` switches to the room view on the player's first menu (for pictures).
