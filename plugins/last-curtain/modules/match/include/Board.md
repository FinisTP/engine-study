# The board (`Board.cpp`)

A grid of `width` x `height` squares from `rules.json` `board`; square `c` is at `x = c % width`,
`y = c / width`. Side 0's hub is in the bottom row, side 1's in the top row. Each square has a kind
(wall, hall, room, centre, hub) and doors (bit 0 north, 1 east, 2 south, 3 west); a step goes
through a door, and `Steps` counts them with a breadth-first search.

Seed 3, drawn by kind (`-` hall, `r` room, `w` weak den, `S` strong den, `e` event, `i` item,
`+` centre, `*` stage room, `D` guardian's square, `H` hub; `=` an east door):

```
  r w=-=H=-   e
S=-=-=-=D=-=-=-=e
  -   w=-   r=-=i
i=-=r   -=w r=-
  -=-=-=-=-=-=-
  - r   -=r   -
  +=+=+=+=+=+=+
  +=*=*=*=*=*=+
  +=+=+=+=+=+=+
  - r   -=r   -
  -=-=-=-=-=-=-
i=-=r   -=w r=-
  -   w=-   r=-=i
S=-=-=-=D=-=-=-=e
  r w=-=H=-   e
```

## How it is built

1. Hub in the middle of the bottom row, a hall on each side of it; above it the guardian's square on a cross hall joining
   the lanes (one, two or three: the middle column and/or columns 1 and `width - 2`).
2. Lanes run up to the centre: three rows of open squares, each joined to all its neighbours.
3. Sometimes a cross link between two neighbouring lanes (a second route).
4. Rooms hang off the halls, one door each, in about seven of ten free places.
5. Dens, events and items go in rooms by their steps from the hub (the bands in `rules.json`);
   events stay `eventGap` steps apart.
6. The bottom half is copied onto the top.
7. Stage rooms are the middle row's squares with four doors; the stage is the middle one. The centre
   spawn squares are on that row, `spawn.centre` steps either side of it.

## Why it is fair

The mirror is `M(x, y) = (x, height - 1 - y)`: a reflection across the middle row, like a
chessboard. Copying a square to `M` keeps its east and west doors and swaps north with south, so
a door between `a` and `b` becomes a door between `M(a)` and `M(b)`. A path from hub 0 to `c`
therefore maps step by step onto a path from hub 1 = `M(hub 0)` to `M(c)`, and back, so

    Steps(hub0, c) = Steps(hub1, M(c))   for every square c.

A square in the middle row is its own mirror, so its steps to both hubs are equal: that is why
the stage and the centre spawn squares come from that row. `Fair` checks all of it square by square rather than trusting the
proof; `BuildBoard` tries up to 64 seeds derived from the given one until a board passes.

## Tiles (2026-11-20, the user's notes)

The plan above is drawn in squares, then `Expand` turns it into tiles, so walls are tiles of
their own and can be seen: square (x, y) becomes tile (2x, 2y); between two squares a tile that
is open (a doorway, of their kind when both share it, else a hall) where a door joins them, a
wall where none does; the corner tile between four squares is open only when the four are joined
all round and of one kind (so the centre and the cross halls stay open floors). Every two open
tiles side by side are then joined. The board grows from `width x height` squares to
`(2 width - 1) x (2 height - 1)` tiles.

The mirror still holds: tile (2x, 2y) mirrors to (2x, 2(height - 1 - y)) = (X, H - 1 - Y) for the
tile board's height H, and a doorway or corner lies between squares that mirror together, so
`Fair` checks the tile board exactly as before. Every step between squares is now two tiles, so
the plan's bands (`weakMin` ...) and `eventGap` are in squares and the gameplay numbers in the
data are in tiles: characters' `move`, monsters' `sight`, board abilities' `range` (all doubled),
and a hunter walks `speed * board.stride` tiles (`stride` 2).

## Sharing a tile

`squareCapacity` (2) of one side may stand on a tile, never with a foe (wild monsters are their
own side); the stage holds one (`Room`).
