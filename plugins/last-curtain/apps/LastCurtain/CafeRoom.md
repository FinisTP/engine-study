# The café drawn (`CafeRoom`)

- **The room** comes from `assets/cafe/room.json` in metres: room x is world x, room depth is
  world z (0 at the back wall). The floor, the papered back and side walls (`art/ui/match/wall.jpg`
  repeating every metre), the counter and the tables as boxes from `blocks`, warm lamps over the
  counter and each table. No front wall: the camera looks in from there.
- **Spots**: stations as coloured boxes on the counter (phin brown, tea green, milk cream, ice
  blue), the sign (green when open, red closed), the bucket, the notice board on the left wall,
  the stage door on the right; the one within reach glows.
- **Figures**: flat sprites standing on the floor, facing the camera. Ghosts sit behind their
  table (a picture of their own when `art/sprites/<guest sprite>/body_calm.png` exists, else the
  shared specter), bobbing a little; the walker bounces as it walks and faces the way it goes.
- **The camera** eases after the walker, kept inside the room.
- **Labels**: each order (drink, patience bar, who) over its ghost; the talk's line over whoever
  says it. `Floor` turns a click into room metres for `CafePlay::WalkTo`.
