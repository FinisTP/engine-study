# MatchTable: the faction match on the table

The board lies sideways, as in the prototype: the board's rows run left to right, side 0's hub (the
board's bottom row) on the left, side 1's on the right. A square is `tile` (1.6 m) across; square
`(x, y)` sits at table `(((height-1)/2 - y) * tile, 0, (x - (width-1)/2) * tile)`. `Pick` casts the
mouse's ray to the table top (y = 0) and turns that point back into a square.

- Squares by kind: halls dark wood, rooms faded carpet, the centre opera red, hubs in their side's
  colour; the stage glows gold. Optional goals are only marked (the user): an unresolved event room
  a small "?" glyph standing in one corner, a treasure a chest glyph in the other; a room coming back
  shows its glyph faint from the round before. Dropped essence is a small gold square.
- Announced spawns: an orange outline pulsing on the square.
- The player's turn (from `MatchPlay::UiData`): the reach in blue (an armed ability's range in
  orange), the path as small dots, targets outlined red, the hovered square outlined white.
- Figures are flat sprites (`art/sprites/<sprite>/body_calm.png`, the `sprite` surface), at each
  picture's own width per height, facing the camera square on so they stand upright on screen, as a
  2D board's figures do. A large monster stands in the middle of its footprint and is taller by its
  size. Characters have an outline in their side's colour under them; side 1's face left.
- `Labels` gives each unit's card point (just over its head) for `match-labels.ui.json`: the acting
  one's arrow, the short name and level, an HP bar, and carried essence in gold.

The camera is the night's `TableCamera`, its distance and tilt from `options.csv`
(`matchCameraDistance`, `matchCameraTilt`); a drag pans, the wheel zooms.

## The look (`assets/match-look.json`)

The post grade (exposure, contrast, saturation, temperature, vignette, bloom) mixes from `grade.start`
at round 1 to `grade.end` at the last round: colder, greyer and darker toward the end, never red (the
user). The lamp stutters by `flicker` (two sine waves beating; only their peaks dim it). Each figure
stands on a shadow: two flat discs, a lighter rim and a darker core, `shadow.width` of a square per
footprint square. They are opaque on purpose: the sprite surface dithers faint alpha into noise.

## Following, the turn ring and scenes

A new turn: a gold ring grows under the one whose turn it is and fades over `ring.seconds`, and the
figure nods. The camera eases toward whoever acts (a battle: the middle of the two), only
`follow.pull` of the way from the board's middle so the board stays in view; a drag stops it until
the next turn. Scenes (`match-look.json`): `open` sweeps down from high and far onto the board when a
match begins; `end` circles the winner's hub when it is over.

## Painted floors and the room view

Every square is painted with one of the night's room pictures (`art/rooms/<name>.jpg`), chosen from
`match-look.json` `rooms` by its kind (hall, room, centre, hub, stage); a square and its mirror share
one. The kind's colour tints it lightly, so the centre still reads warmer. There is no separate
room view to switch to: as the wheel brings the camera from `boardDistance` to `roomDistance`, walls
rise (up to `wallHeight`) on every side of a square without a door, so the board becomes rooms in
one seamless zoom. `--zoom N` starts a capture that many wheel steps in.

## Walking, size and the camera (2026-11-20, the user's notes)

- A figure walks to its new square along the doors (a breadth-first search, display only) at
  `walk.squaresPerSecond`; a jump of more than 12 squares (a respawn) is not walked.
- Figures keep their size on screen: their height grows with the camera's distance
  (`distance / rooms.boardDistance`, within `figures.minScale..maxScale`), so in the room view they
  are people in rooms. They lean back with the camera only `figures.lean` of the way, so a head
  never pokes through the wall behind.
- The camera follows whoever acts all the way (`follow.pull` 1), kept within `follow.edge` of the
  board's middle when far out; whom a told line is about (`focus`) is framed together, the camera
  backing off to `spread * follow.fit + follow.margin` when they are apart.
- The goals are edged: each hub in its side's colour, the centre red where it meets other squares,
  the stage gold; hubs and stage keep a strong tint. Walls are always there, `rooms.wallLow` high on
  the board, papered (`rooms.wallArt`, repeating every `rooms.wallRepeat` metres). The "?" and the
  chest stand in the middle of their room. The reach is a flashing frame, not a fill. A gold arrow
  bobs over the head of the one whose turn it is.

## The battle view

While `MatchPlay` stages a fight the camera comes down (`battle.distance`, `battle.tilt`) and the
screen dips to dark halfway (`battle.fade`); past it the fight has its own stage: the board, the
marks and everyone else are gone, a backdrop by the defender's square kind
(`battle.backdrops`, ComfyUI paintings) stands behind the two, who face each other `battle.gap`
squares apart, `battle.near` metres toward the camera, at `battle.figureScale`. The HUD closes;
the battle screen shows both fighters' cards, the blow and the picks.

## The tile board and the user's second notes (2026-11-20 evening)

- Tiles are 1 m; walls are tiles of their own, papered blocks (`rooms.wallLow` on the board up to
  `rooms.wallHeight` close in, shaded by `rooms.wallShade`). Figure heights keep their old look
  through `figureMetres`.
- Two on a tile stand apart (`walk.shareApart`) and drift back when one leaves.
- The reach (cyan; an armed range orange) and the targets (red) wash their tiles half over,
  blinking (`reach.wash`, `reach.glow`): the picture stays seen.
- Cards and the turn arrow sit on the figure's own head (`Head`, along the sprite's leaning up).
- The camera follows tightly (`follow.seconds` 0.2); an order portrait clicked looks at that one.
- Fights: up to four on the stage, each group on its side, the second a little behind
  (`battle.pairGap`, `battle.pairDepth`); the stage stands where the fight began, so a fallen
  fighter does not move it; a fighter whose fall is not yet told still stands (`SetTold`); the
  one picking has the gold arrow.
