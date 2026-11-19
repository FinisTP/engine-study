# MatchTable: the faction match on the table

The board lies sideways, as in the prototype: the board's rows run left to right, side 0's hub (the
board's bottom row) on the left, side 1's on the right. A square is `tile` (1.6 m) across; square
`(x, y)` sits at table `(((height-1)/2 - y) * tile, 0, (x - (width-1)/2) * tile)`. `Pick` casts the
mouse's ray to the table top (y = 0) and turns that point back into a square.

- Squares by kind: halls dark wood, rooms faded carpet, the centre opera red, hubs in their side's
  colour; the stage glows gold; an unresolved event room has a turning violet diamond.
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
