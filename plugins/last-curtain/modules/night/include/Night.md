# The night, as built (plan 1)

The whole night of *The Last Curtain* without graphics: one copyable `NightState`, and the rules
behind `LegalActions` / `Apply`. Every number comes from `plugins/last-curtain/assets/night/`; the
code holds only the mechanics. Design: `docs/design/game-design.md` (GDD); bots and tuning:
`../../nightbots/include/Balance.md`.

## How a night runs

1. `NewNight(def, seed, player)` builds the deck, puts the foyer bottom centre, places every guest
   there and rolls a d20 each for the order (high first, ties to higher Speed, then lower index).
2. `Current(state)` names who acts: a bystander being asked to join a fight, else the guest fighter
   whose battle turn it is, else a guest with a level to choose, else the board guest. -1 = over.
3. `LegalActions` lists what that guest may do; `Apply` refuses anything else and changes nothing.
4. Monsters never choose: they act inside `Apply`. Bots and people use the same two calls.

## Data: which file owns which number

| File | Owns |
|---|---|
| `rules.json` | clock (start hour, 20 rounds, the Turn after round 10), grid size and rooms placed, check targets, light steps, carry limit, battle formula, level steps, omen targets, monster spawning, persuading, downed HP, bond loss, the balance band |
| `guests.csv` | the seven guests: stats, weapon type, style, temper, Taken form stats and power |
| `bonds.csv` | starting bonds, -1000..1000 |
| `powers.json` | each power's kind and numbers (damage, range, rounds, cooldown) |
| `rooms.csv` | the room set: kind, doors before rotation, bonus stat, how many of each |
| `events.csv` | events and traps: stat, target word, pass and fail effects |
| `items.csv` | items: kind, effect, amount, buff, draw weight, UI line |
| `buffs.csv` | buffs and debuffs: stat, amount, rounds |
| `monsters.csv` | monsters: stats, attack type, resistance, XP, debuff, growth per hour |
| `omens.csv` | omens and their gifts |
| `styles.csv` | bot play styles: weights and thresholds |

`LoadNight` refuses bad data with code `night.<file>` and field `<file>:<row>:<column>`.

## The board (`Board.cpp`)

- Cells are `y * width + x`; directions 0 N, 1 E, 2 S, 3 W; `doors` bit 0 is N.
- `Open` takes the next deck room and rotation that has a door back and leaves an unopened
  doorway somewhere (or no rooms left). Skipped rooms stay in the deck. None fits: the next room
  with four doors. Still sealed: a door is opened from an opened cell to an unopened neighbour.
- The under-stage is placed once a doorway leads 3 or more grid squares from the foyer, as the
  last room at the latest, and at the Turn if nobody found it.
- `Steps` is a breadth-first search through passages. A passage needs both doors and no seal.
  `Lit(viewer, cell, steps)` is "within that many steps"; `InLine` a straight row or column of passages.

## A turn (`Turn.cpp`)

- Moves = Speed. Passage: 1 move. Doorway: opens the room. Item, event, trap and omen rooms stop
  movement until resolved; an unused bonus room stops it too. Transport rooms move you to the
  other one of the pair for 1 move (`Action{Move, transportMove}`).
- Items, drop and pick up are free. Resolving an item, event or trap room ends the turn; so does
  taking or leaving an omen, persuading, or a battle.
- Ending the turn in an unused bonus room adds its stat (once per guest per room).
- `Stat` = base + worn items + omens + battle buffs; in water Speed is 1; the Taken has its form's
  fixed numbers.

## Checks, events, items (`Turn.cpp`)

`RollCheck`: d20 + stat >= target. Events and traps are drawn without replacement; items by
`weight`, without replacement (so each relic at most once a night). A full guest finds the item on
the floor. Before the Turn 0 HP is *down*; a relic wakes a downed guest in the same room.

## Battles (`Battle.cpp`)

```
damage = max(minDamage, base + attack - defense / defenseDivisor)
attack, defense = Might for physical, Sanity for magical (with buffs)
then / 2 if a monster resists that type, - armour against physical, / guardDivisor if Guarded,
and never below minDamage.
```

The side that started acts first, in fighter order; monsters strike the weakest enemy by
themselves. A battle ends after `battle.rounds` rounds, when a side has nobody standing, or on
`Leave` (from round 2). Everyone keeps their HP; the board turn of whoever started is over.
Killing a monster gives its XP to every standing guest on the other side; a level is chosen
(`LevelUp`) before anything else. Monsters appear at round start from `monsters.fromRound`.

## The company (`Company.cpp`)

Attacking a guest: everyone whose light reaches the room loses `attackBondLoss` toward the
attacker; bots in the room not allied to either side are asked to join the defender
(`Battle::asking`); allies of each side in the room fight on it. Only the player persuades:
d20 + Sanity against `persuade.target`, easier or harder by `persuade.bond` with the target's bond.

Omens: the first guest in may take or leave it. Taking rolls d20 + Sanity *before the gift*
against `omens.targets[omensTaken]` (the last repeats); the first failure marks. Only guests
whose light reaches the room learn of it (`Knows`). The marked guest brought down by a guest
before the Turn ends the night (`Outcome::CompanyEarly`).

## The Turn and the hunt (`Hunt.cpp`)

At the end of round `turnAfterRound`: nobody marked means chance picks an omen holder, weighted
by omens held (with none, anyone). The marked guest leaves every group and rises as the Taken in
the under-stage at its form's HP; everyone else gathers in the foyer, the downed stand with
`downedReturnHp`; the Taken moves to the end of the order. After midnight 0 HP is death and items
fall. The house never hurts the Taken; it ignores rooms and monsters; monsters join its side.
A battle starts when the Taken and company guests share a room, with every company guest there.

Powers (one code path per `kind`, numbers from `powers.json`, at most one use per turn, then
`cooldown` Taken turns):

| Kind | Legal when | Effect |
|---|---|---|
| line | a guest is `InLine` | `damage` to that guest |
| area | a guest within `range` steps | `damage` to every guest within `range` |
| flood | always | its room and rooms joined to it: Speed 1, `damage` on ending a turn there, `rounds` |
| pull | a guest 1..`range` steps away | that guest comes to its room, takes `damage`, battle |
| seal | a passage within `range` steps that cuts nobody off from it | shut for `rounds` |
| swap | always, on any guest | the two change places; the Taken moves on |
| reflect | always | the next strike on it, until its next turn, hits the striker |

## Saves and the fingerprint (`NightSave.cpp`)

`SaveNight` writes every field of the state (JSON) with the data's fingerprint, a hash of every
data file's bytes; `LoadNightState` refuses other data, damaged JSON and out-of-range indexes
(code `night.save`). `Fingerprint` hashes the state; the golden value in `tests/SaveTests.cpp`
changes whenever the data or the rules change.

## Randomness

`inner::Dice` is xorshift64*; the seed goes through splitmix64 first so that neighbouring seeds
give unrelated nights. No floats and no standard distributions: the same data and seed give the
same night on GCC, Clang and MSVC.
