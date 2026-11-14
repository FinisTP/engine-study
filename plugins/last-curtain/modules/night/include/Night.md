# The night, as built (plan 1)

The whole night of *The Last Curtain* without graphics: one copyable `NightState`, and the rules
behind `LegalActions` / `Apply`. Every number comes from `plugins/last-curtain/assets/night/`; the
code holds only the mechanics. Design: `docs/design/game-design.md` (GDD); bots and tuning:
`../../nightbots/include/Balance.md`.

## How a night runs

1. `NewNight(def, seed, player)` draws each guest's goal, builds the deck, puts the foyer bottom
   centre and opens one start room per guest through random doorways around it (never the
   under-stage, never a room a drawn goal needs); each guest starts in its own, which gives it
   nothing on that entry (resolved, its bonus used). The rooms the goals need move to random places
   among the next deck entries, so they come out within `goals.earlyRooms` rooms. Then a d20 each
   sets the order (high first, ties to higher Speed, then lower index).
2. `Current(state)` names who acts: the guest fighter whose battle turn it is, else a guest with a
   level to choose, else the board guest. -1 = over.
3. `LegalActions` lists what that guest may do; `Apply` refuses anything else and changes nothing.
4. Monsters never choose: they act inside `Apply`. Bots and people use the same two calls.

## Data: which file owns which number

| File | Owns |
|---|---|
| `rules.json` | clock (start hour, 20 rounds, the Turn after round 10), grid size and rooms placed, check targets, light steps, carry limit, battle formula, level steps, curse targets and their rise per hour, monster spawning, persuading, downed HP, the balance band |
| `guests.csv` | the seven guests: stats, weapon type, style, Taken form stats and power |
| `questions.csv` | five questions per guest: text, a physical and a magical answer, each with its target word |
| `goals.csv` | two or three goals per guest: kind (room, item, events, monsters; no ally goals, user), target, count, text |
| `minions.csv` | each Taken form's minions: stats, attack, resistance, XP, debuff, share of the Taken's HP, how many |
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

- Moves = Speed. Passage: 1 move. Stepping back into the cell a plain last step came from (no
  room opened, nothing rolled, fought or picked up) undoes it: the move comes back, and the cell
  just left is barred for the rest of the turn (`Guest::undo`, `barred`), so no pacing. Doorway: opens the room. Item, event and trap rooms stop
  movement until resolved; an unused bonus room stops it too. Transport rooms move you to the
  other one of the pair for 1 move (`Action{Move, transportMove}`).
- Items, drop and pick up are free. Resolving an item, event or trap room ends the turn; so does
  persuading, or a battle.
- Ending the turn in an unused bonus room adds its stat (once per guest per room).
- `Stat` = base + worn items + battle buffs; in water Speed is 1; the Taken has its form's
  fixed numbers.

## Checks, events, items (`Turn.cpp`)

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

## The company (`Company.cpp`)

Guests never fight each other before midnight; afterwards only the Taken. Allies of the player
in the room fight on the player's side.

Talking (before midnight, once a round each): any guest may `Chat` with a guest in the room
who is not in its group, is not the player and is not one of the player's allies (it uses the turn). The other guest asks it one of its five questions
(`questions.csv`, not one asked before while any remain); a bot may also ask the player at the
start of its turn (`OpenChat`; it does not use the player's turn). The answerer (`Current`) picks
`Answer` 0 (d20 + Might) or 1 (d20 + Sanity) against that answer's target, `persuade.bond` easier
if the asker likes the answerer, harder if not (`AnswerTarget`). A pass is one approval
(`NightState::approvals`, asker * 7 + answerer) and tells the answerer the asker's goal (between
two bots, both ways). At `persuade.approvals` the asker joins the answerer's group, while it has
fewer than 3: the player's group is 0, a bot's is 1 + its index. `ChancePercent(stat, target)` is
what the screens show.

## Goals (`Company.cpp`, `Turn.cpp`, `Battle.cpp`)

Each guest draws one of its own `goals.csv` rows at `NewNight`, before the board; it knows only
its own (`Guest::knows`, bits). A passed chat answer tells the answerer the asker's goal (see Talking). Kinds:
`room` (entering the room sets `goalRoomSeen`), `item` (a keepsake carried at midnight), `events` (passed event rolls, traps not counted) and
`monsters` (ordinary monsters this guest destroyed): `goalCount` against `count`. A keepsake
(`items.csv` kind `keepsake`, weight 0, `room`) lies on the floor of its room when the room is
opened, only if a drawn goal asks for it.

## The Turn and the hunt (`Hunt.cpp`)

At the end of round `turnAfterRound` every living guest's goal is checked (`GoalDone`, a `Goal` log
entry each). If all are done, the company wins at once. Otherwise chance picks the Taken among
the failed, and every other failed guest keeps at most `goals.failHp` HP (the downed are left as
they are). The Taken leaves every group and
rises as the Taken in the under-stage at its form's HP; everyone else gathers in the foyer, the
downed stand with `downedReturnHp`; the Taken moves to the end of the order. Ordinary monsters
vanish and stop appearing; the form's minions (`minions.csv`, `count` of each) wait in the opened
rooms farthest from the foyer. Killing a minion takes `hpShare`% of the form's `takenHp` from the
Taken at once. After midnight 0 HP is death and items fall. The house never hurts the Taken; it
ignores rooms and minions; minions fight on its side. A battle starts when the Taken and company
guests share a room, with every company guest there.

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
