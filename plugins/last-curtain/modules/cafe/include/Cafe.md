# The café (`Cafe.hpp`, `Career.hpp`)

Between matches the characters keep a coffee shop in the opera house, and the ghosts of the
match come in for a drink: a cosy, timed rush after the hard fights (the user, 2026-11-20).

## Data (`assets/cafe/`)

- `stations.csv`: the steps a drink can take (phin, milk, ice, tea), one station each.
- `recipes.csv`: a drink is 1 to `maxSteps` steps in order (`phin;milk;ice` is iced milk coffee)
  and a price.
- `guests.csv`: the ten ghosts (their `monsters.csv` ids and sprites), the drinks each orders,
  its patience in seconds, how often it comes.
- `cafe.json`: the shift's length and arrivals, the seats, the largest tip, steps per cup, and the
  money a match's performed essence is worth.
- `talk.json`: short talks at the counter, by ghost, then by serving character or `any`.

## A shift

`OpenShop` seats no one yet; `Tick` moves the clock. Until `shift.seconds`, a guest comes every
`arrivalMin..arrivalMax` seconds (from the seed) if a seat is free, picked by weight, ordering one
of its drinks. Each seated guest's patience runs down; at nothing it leaves (`Left`, lost). The
player fills one cup at the stations (`UseStation`, at most `maxSteps`), empties it (`Trash`) or
serves it to a seat (`Serve`): the exact steps pay the price plus a tip of
`ceil(patience left / patience * tipMax)`; anything else sends the guest away unpaid (`Wrong`,
lost). After closing time, when the last guest is gone, the shift is over (`Closed`, with the
day's takings and tips). `CloseShop` ends it at once; the seated simply leave.

## Career

`career.json` (version 1, written to a temp file and moved): money, the day, each character as it
left its last match (level, XP, stats, abilities by id, upgrades), and the talks already heard. A
version that is not 1, or a broken file, is refused and the career in memory is left as it was.
