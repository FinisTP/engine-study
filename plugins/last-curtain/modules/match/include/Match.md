# The faction match, as built

The rules of `docs/design/2026-11-19-factions.md`, headless: `MatchDef` is the data, `MatchState`
one copyable value, and only `Apply` changes it. Bots and the app send the same `Action`s.

## Data: which file owns which number (`assets/match/`)

| File | Holds |
|---|---|
| `options.csv` | `id,value`: rounds (the countdown), side (characters per side), autoEndTurn (0/1), bots (easy, medium, hard) |
| `rules.json` | board (size, lanes, one unit per square, den and event distances, counts per half), stage (rounds open), respawn (turns away), battle (rounds, minimum damage), levels (XP steps, HP per level, XP for a kill), great monster (wake round), carry, bot noise per level, the soak's balance bands |
| `characters.csv` | id, name, role, the five stats (hp, attack, defence, speed, move), sprite, tint |
| `abilities.csv` | id, name, use (battle, board, passive), class (strike, shield, heal, hit, firststrike), hpCost, power, dice (`2d6` or none), range, animation (a `motions.json` name) |
| `jobs.csv` | role, level, ability, choice (a second ability to pick between, or none); level 1 = starting abilities |
| `monsters.csv` | id, name, tier (weak, strong, great, guardian), behaviour (stay, flee, stage, guard), stats, essence, xp, respawnRounds, abilities (`a;b`, the first is its strike), sprite, tint |
| `items.csv` | id, name, text, sprite, class (heal, xp), amount |
| `events.csv` | id, name, text, two choices: label, chance (percent), success, failure (`none`, `hp:-3`, `essence:2`, `xp:2`, `item:<id>`) |

The loader refuses a role without a free (0 HP) level-1 Strike, so a fighter at 1 HP can always act.

## A new match (`Match.cpp`)

`NewMatch(def, seed, picks)`: the board from the seed; side 0's characters as picked, side 1's
from `RiggedPick` (no role twice, Defender, Supporter, All-Rounder and Attacker before the
Speedster; a character may be on both sides). Characters start on the halls nearest the hub
(not the hub itself, which stays free), side 1 on the mirror squares. `order` is every character by Speed, highest first, ties by a
key drawn from the seed; it never changes. Each side's guardian stands on its `door` square with
that side's faction; each den gets a monster of its tier, the same kind on both halves; each item
room an item. The stage opens in a random stage room and closes at round `1 + stage.openRounds`.
`units` holds side 0's characters, then side 1's, then the monsters.

## A turn (`Turn.cpp`)

`Current` is the unit to act: one with a level choice waiting, else the fighter to pick in a
battle, else `order[turn]`. A turn is move (one step per point of `move`, through a door, onto a
free square; an enemy hub is shut while its guardian stands), `Undo` back to where the turn began
(until the action), one action, then `EndTurn` (or automatically, with `autoEndTurn`). Standing on
your own hub when the move is committed (the action or the end of the turn) restores full HP, so
stepping in and undoing heals nothing. After the last character in `order`, `RunMonsters`, then a
new round; after round `rounds` the match ends: most score, then the side whose guardian alone
stands, then more kills, else a draw (`winner` -1). `Check` gives the reason (`Why`) an action
cannot be taken, for the app's tooltips.

## Battles (`Battle.cpp`)

`Attack` a unit exactly one step away, not of your faction. A battle is `battle.rounds` rounds; in
each the attacker picks a battle ability, then the defender, unless only the defender has a
FirstStrike passive. Monsters pick their first affordable Strike at once.

```
damage = power (or the dice roll) + Attack - (Defence + the defender's shield this battle)
         at least battle.minDamage
```

An ability is usable only while HP is above its cost. Shield adds its power to the side's shield;
Heal adds power HP. At 0 HP a unit is down and the battle ends. A character downed by a character
gives it all its essence (and `levels.killXp` XP, a kill); by a monster, drops it on the floor.
A monster downed by a character gives its essence and XP; weak ones return to their den after
`respawnRounds`. A downed character leaves the board for `respawn.turns` of its turns, then stands
on its hub (or the nearest free square) with full HP. `Preview` is the forecast: damage range,
healing and cost.

## Levels, abilities, items, events (`Abilities.cpp`)

XP comes from monsters (their `xp`) and character kills (`levels.killXp`), items and events. Each
`levels.xp` step passed raises the level and max HP (`levels.hp`) and teaches the job's row for
that level; a row with a `choice` waits (`pending`) for `LevelUp` 0 (the ability) or 1 (the
choice), which comes before any other action, even for a downed character. A board ability is the
turn's action: Hit an enemy unit, or Heal a character of your side (yourself too), within `range`
steps; it costs `hpCost` and needs HP above it. `UseItem`: heal items heal, XP items give XP.
`Answer` on a square with an unresolved event rolls the choice's chance (percent) and applies its
success or failure; the event is then spent. An event that takes the last HP downs the character,
and its essence drops where it stood.

## The stage (`Stage.cpp`)

The stage is one of the board's stage rooms and holds one unit, whatever `squareCapacity` says.
Standing there scores nothing. `Perform`, the turn's action on the stage with essence carried,
adds it all to the side's score. At the start of round `stageCloses - 1` the next stage room is
announced (`StageNext`, never the current one); at round `stageCloses` the stage moves there
(`StageOpen`) and closes again `stage.openRounds` later. A unit on the old stage stays put.

## Monsters (`Monsters.cpp`)

After the last character in a round, in unit order, every monster on the board acts by its
behaviour: Stay does nothing; Flee takes up to `speed` steps away from the nearest character;
Stage takes up to `speed` steps toward the stage and stands on it when it is free; Guard deals its
first ability's damage to every enemy character next to it, without a battle (guardians never
start battles), which can down them. Monsters fight back only when attacked. A unit cannot attack
a monster of its own faction. A downed weak monster returns to its den, whole, at `respawnRound`
if the den is free; guardians and the great monster never return. The great monster waits off the
board and wakes at `great.wakeRound` in a free stage room that is not the stage.

## Saves (`MatchSave.cpp`)

`SaveMatch` writes every field of `MatchState` by name, the board and the log included, with
`"format": "umbra.match", "version": 1`. `LoadMatch` refuses another format or version and any
missing or mistyped field, with a reason, and leaves the state it was given untouched. A save taken
in the middle of a battle or a level choice loads to the same moment. `Fingerprint` hashes
everything but the log, for tests. A new field means a new version.
