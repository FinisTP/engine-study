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
