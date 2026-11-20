# The café on screen (`CafePlay`)

The headless half of the hub (the room draws itself in `CafeRoom`); tested like `MatchPlay`.

- **Walking.** One character (the walker, chosen at the notice board) walks the floor of
  `assets/cafe/room.json`: WASD moves it (`Move`, each axis on its own, so it slides along the
  counter), a click walks it there (`WalkTo`, stopping if a block is in the way). The room's walls
  and its `blocks` (the counter, the tables) are never entered, with a quarter metre to spare.
- **Spots.** E does what the nearest spot within `reach` offers: the sign opens the shop for the
  day's shift (once a day) or closes it; a station adds its step to the cup; the bucket empties
  the cup; a table with a guest takes the cup (`cafe::Serve`); the notice board shows the company;
  the stage door leads to the next match once the shop is closed. `UiData()["prompt"]` says what E
  would do there; `hint` what to do next.
- **The day.** When the shift is over (closing time, or the sign) the takings and tips land in the
  career once and the takings screen shows; after it the sign says the shop is closed for today.
- **Talks.** A served ghost talks with the walker: its first talk with that character not yet
  heard, else its first with anyone; each is told once (`career.heard`), a line every
  `lineSeconds`, in a bubble over the table or the walker.
- **The board.** Every character: portrait, level and abilities as the career keeps them (or the
  role's first ones), and "Walk as" to change the walker.

`CareerMatch` is the bridge to the match: `ApplyCareer` starts the player's side as the career
left it, `RecordMatch` writes it back and adds the takings (side 0's score times
`moneyPerEssence`) and a day.
