# The night, as built (the battle royale, 2026-11-16)

The whole night of *The Last Curtain* without graphics: one copyable `NightState`, and the rules
behind `LegalActions` / `Apply`. Every number comes from `plugins/last-curtain/assets/night/`; the
code holds only the mechanics. Design: `docs/design/2026-11-16-battle-royale.md` (the co-op game it replaced stays on branch `main`); bots and tuning:
`../../nightbots/include/Balance.md`.

## How a night runs

Seven guests are locked in the Saigon Opera House; the last one alive walks out (the user, 2026-11-16).

1. `NewNight(def, seed, player)` builds the whole map at once (`BuildBoard`: the quest rooms first
   in the deck, then the rest, up to `board.rooms`; carry-quest items laid in their rooms) and spawns
   the guests far apart (`Spawns`: farthest-point picking, never the under-stage). A d20 each sets
   the order.
2. `Current(state)` names who acts: the fighter whose battle turn it is, else a guest with a level to
   choose, else the board guest. -1 = over.
3. `LegalActions` lists what that guest may do; `Apply` refuses anything else and changes nothing.
4. Monsters and the killer never choose: they act inside `Apply`. Bots and people use the same calls.
5. A night lasts `clock.rounds` rounds. One guest left alive: they escape (`Outcome::Escaped`, the
   `winner`). None: the house wins (`Outcome::House`; a ghost player wins with it). Two or more at
   dawn: `Dawn` starts the next night (`NightState::night`) on a new map; the living keep their
   items, levels and powers and wake healed and far apart; the quests open again.

## Data: which file owns which number

| File | Owns |
|---|---|
| `rules.json` | clock (start hour, rounds, minutes a round), ghosts, spawn distance, the killer, grid size and rooms placed, check targets, light steps, carry limit, battle formula, level steps, monster spawning and its rise per hour, persuading, the balance band |
| `guests.csv` | the seven guests: stats, weapon type, style, `nature` (kind, dirty, wicked) |
| `quests.csv` | the night's quests: kind (carry, visit, slay, endure), rooms, item, count, the power and stat they give, text |
| `questions.csv` | five questions per guest: text, a physical and a magical answer, each with its target word |
| `goals.csv`, `minions.csv` | the co-op game's goals and the Taken's minions: still loaded, unused |
| `bonds.csv` | starting bonds, -1000..1000 |
| `powers.json` | each power's kind and numbers (damage, range, rounds, cooldown) |
| `rooms.csv` | the room set: kind, doors before rotation, bonus stat, how many of each |
| `events.csv` | events and traps: stat, target word, pass and fail effects |
| `items.csv` | items: kind, effect, amount, buff, draw weight, UI line |
| `buffs.csv` | buffs and debuffs: stat, amount, rounds |
| `monsters.csv` | monsters: stats, attack type, resistance, XP, debuff, growth per hour |
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

- **Moves** (user, 2026-11-15): a turn starts with `budget` = Speed at `origin` = where the guest
  stands. What is left is always the budget less the shortest way back to the origin through
  passages (`Distance`, a breadth-first search), so walking out and back costs nothing and a misclick
  can be walked back. A step is legal if it keeps that way within the budget; a doorway (a new room)
  needs one move left. Two things **commit** (the room becomes the origin, with what is left as
  the budget): opening a room, and a room that stops the guest (item, event and trap rooms until
  resolved, an unused bonus room, a fight; `Stop`). Transport rooms join the pair as a passage of 1
  (`Action{Move, transportMove}`). `rules.json` `moves.stepsPerSpeed` x Speed caps the steps of a
  turn. `Guest::visited` holds the rooms a guest has stood in this turn; bots never walk into one
  again (they head somewhere; no pacing).
- Items, drop and pick up are free. Resolving an item, event or trap room ends the turn; so does
  persuading, or a battle.
- Ending the turn in an unused bonus room adds its stat (once per guest per room).
- `Stat` = base + worn items + battle buffs; in water Speed is 1; a ghost has `ghost` stats.

## Checks, events, items (`Turn.cpp`)

**Persistent events and traps** (user, 2026-11-15): every trap, and the events marked `persist` in
`events.csv`, stay armed in the room they were drawn in (`Cell::event`) and catch every guest who
walks in (the turn ends there, as on the first entry). A passed roll clears it for everyone
(`LogKind::Cleared`); so does walking in while meeting its `clear` condition, without a roll:
`might>=N` / `sanity>=N` / `speed>=N` (the stat with items and buffs) or `carry:<item>`. Clearing an
event (not a trap) either way counts as a passed event for an `events` goal. Events not marked happen
once, as before. Save version 3 carries `Cell::event`.

`RollCheck`: d20 + stat >= target. Events and traps are drawn without replacement; items by
`weight`, without replacement (so each relic at most once a night). A full guest finds the item on
the floor. Before the Turn 0 HP is *down*; a relic wakes a downed guest in the same room. An event outcome `back` pushes the guest to the room
they came from (`Guest::from`).

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

## Fights and persuading (`Company.cpp`)

Any guest may attack any other guest or ghost in its room, at any hour (the battle royale); spawns
are `spawn.minSteps` apart so nobody dies in the first turn.

Persuading (once a round): a living guest asks another for one thing, a `Chat` with a request in
`b`: -1 spare me (a truce, `persuade.truceRounds` rounds: neither attacks the other), -2 an item
(they drop one they carry), or a guest index (they go after that guest, `Guest::hunt`, for
`truceRounds`). The one asked puts one of its five questions (`questions.csv`); the asker answers
with Might or Sanity against that answer's target, `persuade.bond` easier if the one asked likes
them, harder if not, and moved by its nature (`kindTarget`, `wickedTarget`; `AnswerTarget`). A pass
grants the request. A bot may ask the player too (`OpenChat`, before it moves; a pass is a truce).
The rules record truces; keeping them is the bots' choice (`Balance.md` "Natures").

## Quests (`Quests.cpp`)

Every quest is open to every living guest; the first to finish one takes it for the night
(`NightState::questBy`, guest + 1), with its power (replacing any other, cooling from now) and its
stat. Kinds: `carry` (the item in the bag, into the second room; the item is handed over), `visit`
(the first room, then the second), `slay` (monsters destroyed) and `endure` (events and traps
passed), counted in `Guest::quest`. Ghosts do not quest.

## Powers (`Hunt.cpp`)

A guest's power (numbers from `powers.json`, one use a turn, then `cooldown` of its turns):

| Kind | Legal when | Effect |
|---|---|---|
| line | a guest is `InLine` | `damage` to that guest |
| area | a guest within `range` steps | `damage` to every other guest within `range` |
| flood | always | its room and rooms joined to it: Speed 1, `damage` on ending a turn there, `rounds` (`Mark.by` spares the user) |
| pull | a guest 1..`range` steps away | that guest comes to its room, takes `damage`, battle |
| seal | a passage within `range` steps | shut for `rounds` |
| swap | always, on any guest | the two change places |
| reflect | always | the next strike on it, until its next turn, hits the striker |

## Ghosts and the killer (`Turn.cpp`, `Killer.cpp`, `Battle.cpp`)

**Ghosts.** 0 HP is death: items fall, and the guest rises next round as a ghost of the house on
the under-stage with `ghost` stats. A ghost may attack the living; struck down, it leaves the board
and comes back after `ghost.respawnRounds`. `Living` = not dead; `InPlay` = living, or a ghost on
the board. The `Died` log entry has `a` 1 for a living guest and the room in `b`.

**The killer.** It wakes in the under-stage at round `killer.wakeRound` (`Killer` entry, `b` 0) and
can never be destroyed, only escaped. Each round it walks `speed` rooms, +1 every `speedPerHours`
hours since it woke (half in water), toward the nearest living guest within `senseSteps`, else at
random (`Killer` entries, `b` 1). Entering a room it kills its weakest living guest and stops; a
guest who walks into its room dies. Ghosts are untouched. Seal shuts it out; flood slows it. The
screens warn within `hearSteps`.

**Monsters** appear from round `monsters.fromRound` (chance and cap rise per hour:
`chancePerHour`, `maxPerHours`).

## Saves and the fingerprint (`NightSave.cpp`)

`SaveNight` writes every field of the state (JSON) with the data's fingerprint, a hash of every
data file's bytes; `LoadNightState` refuses other data, damaged JSON and out-of-range indexes
(code `night.save`). `Fingerprint` hashes the state; the golden value in `tests/SaveTests.cpp`
changes whenever the data or the rules change.

## Randomness

`inner::Dice` is xorshift64*; the seed goes through splitmix64 first so that neighbouring seeds
give unrelated nights. No floats and no standard distributions: the same data and seed give the
same night on GCC, Clang and MSVC.
