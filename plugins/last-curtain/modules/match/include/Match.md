# The faction match, as built

The rules of `docs/design/2026-11-19-factions.md`, headless: `MatchDef` is the data, `MatchState`
one copyable value, and only `Apply` changes it. Bots and the app send the same `Action`s.

## Data: which file owns which number (`assets/match/`)

| File | Holds |
|---|---|
| `options.csv` | `id,value`: rounds (the countdown), side (characters per side), autoEndTurn (0/1), bots (easy, medium, hard) |
| `rules.json` | modes ("Modes"), board (size, lanes, one unit per square, den and event distances, counts per half), respawn (turns away), battle (rounds, minimum damage), levels (XP steps, HP per level, XP for a kill), spawn (`warn`: rounds a spawn square is marked first; `centre`: steps from the stage to each centre spawn square), carry, bot noise per level, the soak's balance bands |
| `characters.csv` | id, name, role, the five stats (hp, attack, defence, speed, move), sprite, tint |
| `abilities.csv` | id, name, use (battle, board, passive), class (strike, shield, heal, hit, firststrike; revive, remove: "Falls"; giveap: "Biding"), ap (action points it costs, board only), target (enemy, ally, self, monster, room, any: a strike aims at an enemy, a shield or battle heal at yourself, a board heal at your side, a hit at enemy, monster or any), hpCost, power, dice (`2d6` or none), range, animation (a `motions.json` name) |
| `jobs.csv` | role, level, ability, choice (a second ability to pick between, or none); level 1 = starting abilities |
| `monsters.csv` | id, name, tier (weak, strong, great, guardian), behaviour (stay, flee, stage, guard, hunt), stats, essence, xp, level (shown to the player), size (1-3 squares a side), sight (steps a hunter sees; 1+ for hunt), abilities (`a;b`, the first is its strike), sprite, tint |
| `spawns.csv` | zone (weak, strong, centre), from (first round), monster (never a guardian), every (rounds after its square empties; 0 = once); loaded sorted by zone, then from |
| `treasure.csv` | id, kind (item, essence, xp, heal), item (or none), amount, weight: what an item room's face-down cards may hold |
| `levelcards.csv` | id, kind (learn, stat, upgrade), what (hp, attack, defence, speed, move; power or cost for an upgrade), amount (a stat's), weight |
| `items.csv` | id, name, text, sprite, class (heal, xp, revive: "Falls"), amount, use (where it can be used: `cafe`, `board`, `fight`, joined by `;`), family (`item`, `ingredient`, `keepsake`: "Found") |
| `events.csv` | id, name, text, two choices: label, chance (percent), success, failure (`none`, `hp:-3`, `essence:2`, `xp:2`, `item:<id>`) |

The loader refuses a role without a free (0 HP) level-1 Strike, so a fighter at 1 HP can always act.

## Modes (campaign plan 2)

The same rules run the faction match and the campaign's fights. `MatchState::mode` holds the
switches, copied at the start from `rules.json` `"modes"` (`faction` by `NewMatch`; a campaign stage
takes `campaign` and may change it, plan 4):

| Switch | Faction | Campaign | Off means |
|---|---|---|---|
| `essence` | on | off | no essence dropped, carried, banked or scored |
| `perform` | on | off | `Perform` is refused (`Why::Off`) |
| `respawn` | on | off | no return at the hub, no den respawns, no hub healing |
| `fightXp` | on | off | no XP (so no level choice) inside a fight; the campaign pays XP per stage |
| `biding` | off | on | no `Bide` or `Extra` ("Biding") |
| `fallsStay` | off | on | a unit at 0 HP keeps its square, fallen ("Falls") |
| `playerLoss` | off | on | the unit flagged `flag::Player` falling loses the fight |
| `mustFree` | off | a stage's | the win also needs every recruit freed ("Freeing and the end") |
| `win` | `score` | `boss` | score after the round limit, or every boss down |
| `leaders`, `leadRounds` | a campaign stage | none, 0 | these units act first, in order, for the first rounds (tutorials) |
| `roundLimit` | `options.csv` rounds | 0 | 0 = no limit |
| `maxAp`, `apPerTurn` | | 2, 2 | AP stored at most, spent at most a turn |

A unit is `Up` when it is on the board with HP left; `Fallen` when it is on the board at 0 HP (only
with falls). Without falls a unit at 0 HP leaves the board, so `Up` is the old `cell >= 0`.

## A new match (`Match.cpp`)

`NewMatch(def, seed, picks)`: the board from the seed; side 0's characters as picked, side 1's
from `RiggedPick` (no role twice, Defender, Supporter, All-Rounder and Attacker before the
Speedster; a character may be on both sides). Characters start on the halls nearest the hub
(not the hub itself, which stays free), side 1 on the mirror squares. `order` is every unit by Speed ("The order"). Each side's guardian stands on its `door` square with
that side's faction; every spawn square (the dens and `centreSpawns`) gets its zone's
kind for round 1 (see Spawns); each item room an item. The stage is the centre of the middle row
and never moves.
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

## The order (campaign plan 2)

Every unit, monsters included, acts in one order by Speed, in both modes (the monsters' phase is
gone). `order` holds every unit; `BeginMatch` sorts it and `NewRound` sorts it again at each round's
start (Speed, highest first; ties by a key drawn for each unit from `orderSeed`, the same every round),
so a spawned monster or a level card's Speed takes its place. `NextTurn` walks the order and skips a
unit that is not `Up` or whose `from` is later than this round: a unit placed mid-fight (a spawn, a
revive, a return home) gets `from = round + 1` and acts from the next round. A downed faction
character still counts down `back` at its own place. A monster acts by its behaviour at its place
(`MonsterAct`) and the order goes on; a battle it starts waits for the picks.

## Falls (campaign plan 2)

With `fallsStay`, `Down` leaves a unit on its square at 0 HP, **fallen**: it loses its AP, is skipped
in the order, covers no square (so it never blocks and doesn't count toward `squareCapacity`), and
every attack, strike, hit, heal or give-AP refuses it (`Why::Fallen`). A battle ends when one group
has no fighter `Up`. Range to a fallen unit is measured as usual (`Squares` is where a unit lies).

- **Revive** (ability class `revive`, board, target `ally`, `power` = percent of max HP; item class
  `revive`, `amount` = percent, used on a fallen one of your side beside you as `UseItem` with a
  target): stands it up with at least 1 HP, AP 0, acting from the next round. It stands on its own
  square if there is room for one more of its side, else on the nearest square with room (`Steps`),
  ties nearest the reviver, then the lower cell index (`RevivePlace`); no square with room: `Blocked`.
  Either side revives its own; a strung recruit stands up still strung.
- **Remove** (class `remove`, board, target `enemy`): a fallen foe leaves the fight for good
  (`cell` -1, `flag::Removed`); nothing can revive it (`Why::NotFallen`).

## Biding (campaign plan 2, `biding` on)

**Off in the shipped data for now** (the user, 2026-11-24: `rules.json` campaign `biding: false`). The
rules below stay and their tests switch it on themselves.

- **Bide** (`ActionKind::Bide`): the whole turn, before moving or acting; the unit gains 1 action
  point (AP), at most `maxAp` (2). No Defence bonus. Refused once it has moved or acted
  (`AlreadyActed`), and in the faction match (`Off`).
- **Extra** (`ActionKind::Extra`): after the turn's action, 1 AP buys another full move and action;
  at most `apPerTurn` (2) AP spent in a turn (`TurnState::apSpent`), so at most three actions.
- **AP abilities** (`abilities.csv` column `ap`, board abilities only): the ability's AP is paid
  first, from the same pool and the same limit. With 2 AP: a normal turn and 2 extras; a 1-AP ability
  and 1 extra; or a 2-AP ability alone. Without the AP: `NoAp` (and `Legal` leaves it out).
- **Give AP** (class `giveap`, board, target `ally`): moves 1 of the user's AP to an ally that has
  room (`NoAp` otherwise). It counts as spending.
- AP is 0 when a fight begins (`BeginMatch`) and lost when a unit falls. Monsters don't bide yet
  (their behaviours come with campaign plan 4).

## Hasten (campaign plan 4)

- `hasten` (a board ability, Tí's Đưa tin): the target, an ally (never the user), gets **a whole extra
  turn right after the user's**: its id is put into this round's `order` just after the user's place. Its
  own place stays, so it may act twice this round; the next round's sort has it once again.
- `afterhasten` (a passive): after the user hastens someone, its **next turn has `power` more moves**
  (`Unit::bonusMove`, spent by that turn).

## Found: ingredients, keepsakes and drops (campaign plan 4)

- Items of family `ingredient` or `keepsake` never go into a slot: picking one up (never refused for a full
  bag) puts it in `MatchState::found`, which the campaign moves to the café's storage when the stage is won.
- `MatchState::drops` (from the stage file): when a unit falls, each of its drops is rolled
  (`Roll(s, 100) < percent`) and goes to `found`.
- An item whose `use` lacks `board` is refused on the board (`Why::NotHere`).
- A campaign fight seeds its random numbers from the fight's seed in `BeginMatch` (the faction match's
  come from `NewMatch`), so the same seed gives the same drops.

## Arrivals (campaign plan 4)

A stage's reinforcements wait in `MatchState::arrivals` (unit, square, round). At the start of the round
before theirs the log says `Arriving` (the mark on the square); at their round's start they stand on their
square, or the nearest one with room, log `Arrived`, and act from the next round. `noRevive` (bit
`1 << side`): that side's revive abilities and items are refused (`Why::Off`), as nobody on the Rat's side
revives in Stage 1.

## Effects (campaign plan 4)

`abilities.csv` columns `effect` and `amount`; classes `effect` (a board ability that only does its
effect, aimed at an enemy or an ally) and `passive` (always on). Each upgrade version is its own row.

| effect | On | What it does |
|---|---|---|
| `mark` | a strike or an effect | the target takes `amount` more damage until its next turn |
| `alarm` | a strike or an effect | the target has `amount` less Speed until its next turn (the next round's order) |
| `firstshot` | a strike | `amount` more power against a unit whose place in this round's order is still to come |
| `steal` | an effect | a random item from the target into `MatchState::stash`; it drops on the thief's square when the thief is hit |
| `dart` | a passive | after a battle, up to `amount` squares to where the nearest foe is farthest |
| `speedkill` | a passive | blows deal `amount` times the damage to a slower target |
| `deathpaper` | a passive | a kill heals `amount` |
| `rice` | a revive | a fallen ally stands at `power`%; one standing heals `amount` and loses a mark or a slow |
| `hastemove`, `hasteheal` | a hasten | the ally also gets `amount` more moves, or heals `amount` |
| `footsteps` | a passive | an ally ending its turn beside the unit gets `amount` more moves next turn |
| `seawind` | an after-hasten | a name for Chân sáo's version (its `power` is 4); its unit takes no hazard damage |

## Gimmicks (`Fight.cpp` `RunGimmicks`, campaign plan 4)

`MatchState::gimmicks` holds a stage's tricks; the campaign fills it. At each round's start:

1. Last round's marks act. A hazard hurts whoever stands on it by `hazardDamage` (not a SeaWind unit).
   A breath hurts the other side on its row by `breathDamage`. A window lets `spawn` out (flag
   `Spawned`, acting from the next round, with `spawnDrops`) while fewer than `spawnMax` spawns stand.
2. New marks are laid for next round: `hazards` random open squares; a breath on the breather's row every
   `breathEvery` rounds from round `breathFirst` (default 1) (log `Breath`); the windows named by `pattern[(round-1) % size]`.

So every hazard, breath and window is shown one round before it acts. A stolen item is parked on the
nearest window when there is one.

## Freeing and the end (campaign plan 2, `win` boss)

Flags on units (`flag::`): `Player` (the living player), `Boss`, `Recruit` (a gold knot on its strings),
`Strung` (still on them), `Freed`, `Removed`.

- **Cut** (`ActionKind::Cut`, a = unit): only the player (`OnlyPlayer`), as the turn's action, beside a
  *fallen* (`NotFallen`) strung recruit (`NotStrung`). The recruit is freed and leaves the board at once
  (the user, 2026-11-23: it waits in the café); logged `Freed` (the player, the recruit, its square) for
  the café to remember. Until cut, its own side may revive it, still strung.
- **The end** (`CheckEnd`, after every down, removal and cut), in this order: the player fell
  (`playerLoss`) → lost, `Loss::PlayerFell`; nobody of side 0 stands → `SideDown`; with `mustFree`, a
  recruit removed, or every boss down while a recruit is still strung → `Trapped` (Stage 1: the Rat
  falls with Tí on his strings); every boss down → won (`winner` 0). The faction match keeps its
  scoring after the round limit.
- **The rite** (`ActionKind::Rite`): offered only after a loss by the player falling, while
  `riteReady` and not `riteUsed` (`Current` is then the player; otherwise `NoRite`). Every unit of
  side 0 not removed stands up whole (the fallen placed as by a revive, the player first); a battle in
  progress ends; `riteUsed` is set and stays set (plan 5 saves it at once); the order goes on from
  where it stopped.

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
A monster downed by a character gives its essence and XP and never returns; its square spawns a
new one (Spawns). A downed character leaves the board for `respawn.turns` of its turns, then stands
on its hub (or the nearest free square) with full HP. `Preview` is the forecast: damage range,
healing and cost.

## Levels, abilities, items, events (`Abilities.cpp`)

XP comes from monsters (their `xp`) and character kills (`levels.killXp`), items and events, and
every board ability used, hit or heal (`levels.abilityXp`; a heal is logged `Healed`: healer, healed, HP back). Each
`levels.xp` step passed raises the level and max HP (`levels.hp`) and teaches the job's row for
that level; a row with a `choice` waits (`pending`) for `LevelUp` 0 (the ability) or 1 (the
choice), which comes before any other action, even for a downed character. A board ability is the
turn's action: Hit an enemy unit, or Heal a character of your side (yourself too), within `range`
steps; it costs `hpCost` and needs HP above it. `UseItem`: heal items heal, XP items give XP.
`Answer` on a square with an unresolved event rolls the choice's chance (percent) and applies its
success or failure; the event is then spent. An event that takes the last HP downs the character,
and its essence drops where it stood.

## The stage (`Stage.cpp`)

The stage is the centre of the middle row, fixed for the match, and holds one unit, whatever
`squareCapacity` says; monsters never stand on it. Standing there scores nothing. `Perform`, the
turn's action on the stage with essence carried, adds it all to the side's score.

## Monsters (`Monsters.cpp`)

A monster on the board acts at its own place in the order ("The order"), by its behaviour: Stay does nothing; Flee takes up to `speed` steps away from the nearest character;
Stage takes up to `speed` steps toward the stage and stops beside it; Hunt finds the nearest
character within `sight` steps (to any of its squares), walks up to `speed` steps toward it on
centre squares only, and when beside it starts a battle as the attacker; Guard deals its
first ability's damage to every enemy character next to it, without a battle (guardians never
start battles), which can down them. Monsters fight back only when attacked. A unit cannot attack
a monster of its own faction. A downed monster never returns.

A hunter's battle is its turn: `Current` is the character whose side must pick, and nothing
else can be done. When that battle ends (`EndBattle`), the order goes on from the next unit. Every monster's battle pick is `MonsterPick`
(`MonsterMind.cpp`): each usable ability gets a value, the highest wins, ties to the first slot. A
strike is worth its damage (dice at their average), or 1000 if it downs the foe; a shield is worth
`50 + power` only when the foe's hardest next blow would down the monster; a heal `50 + HP healed`
only when the monster is below half HP or would be downed. Guardians' blows outside battle are not
picks.

## Large monsters (`Board.cpp` `Squares`, `Covers`)

A unit of `size` s (from `monsters.csv`) stands on its `cell`, the top-left square, and covers the
s x s squares from there; characters are size 1. A covered square counts as occupied: nobody steps
onto it. A monster is placed, spawned or moved only where every covered square is on the board,
open (not a wall), not the stage and covered by nobody else (`Fits`). "Next to" (attack, a guard's
blow) means any square of one beside any square of the other (`Beside`); a board ability's range
counts steps to the target's nearest square (`StepsTo`). A spawn is centred on its square: the
anchor is `(size - 1) / 2` squares up and left of it, so a 3x3 on a centre spawn square fills three
rows of the centre and a 2x2 takes the square and those right of and below it.

## Spawns (`Monsters.cpp`)

Spawn squares are the weak dens (zone 0), the strong dens (zone 1) and `centreSpawns` (zone 2:
the middle row, `spawn.centre` steps either side of the stage, each its own mirror). A zone's kind
at round `r` is its latest `spawns.csv` row with `from <= r`. Each square owns one monster slot in
`units`. A den's monster belongs to the side whose half the den is in (side 0's half is the bottom),
so only the other side can fight it; the centre's belong to no one. A row with `every` 0 spawns once
and never again (the centre's). When its monster is downed, the square schedules a `Spawn` for round `now + every` (the
row in force now) of the kind for that round; if its mirror is already due, it takes the same
round, so mirrored squares spawn together. `spawn.warn` rounds before, it is announced
(`SpawnSoon`: square, kind, round). At the start of its round, or after any later turn once it fits, the new monster replaces the slot, whole (`Spawned`). A zone whose first
row starts after round 1 leaves its squares empty until then.

## Saves (`MatchSave.cpp`)

Version 5 (campaign plan 4): `Unit::bonusMove`, `found`, `drops`.

`SaveMatch` writes every field of `MatchState` by name, the board and the log included, with
`"format": "umbra.match", "version": 2` (2: spawns, the fixed stage). `LoadMatch` refuses another format or version and any
missing or mistyped field, with a reason, and leaves the state it was given untouched. A save taken
in the middle of a battle or a level choice loads to the same moment. `Fingerprint` hashes
everything but the log, for tests. A new field means a new version.

## Treasure (`Treasure.cpp`)

Every item room holds a treasure (`Floor::treasure`). Picking it up deals `treasure.min` to
`treasure.max` face-down cards (`MatchState::cards`), each a `treasure.csv` row drawn by weight. The
opener must then pick one (`Card`, a = its index; anything else is `MustChoose`): an item goes in the
bag (or gives `treasure.fullEssence` if the bag is full), essence is carried, XP is gained, HP is
healed. The cards are then gone (`Treasure` log: who, the row, the amount) and the action is spent.
Bots pick blind.

## Rooms come back (`Treasure.cpp`)

A claimed treasure (picked up) or event (answered) schedules a `RoomSpawn` `rooms.respawn` rounds on,
in a random free room of the same half: a room square (never the centre or the stage), not a den, with
no event or treasure there and none already coming. At the start of its round it opens: a new event
drawn from `events.csv`, or a fresh treasure. The table marks a coming room a round ahead. No traps
(the user).

## Level cards (`Abilities.cpp`)

Each XP step passed is one level: more HP (`levels.hp`), then `levels.cards` cards drawn by weight
from `levelcards.csv`, never the same card twice (`MatchState::offer`; the unit is `pending`):
- **learn** an ability of the role (a `jobs.csv` row of level 2 up to the unit's level, its ability
  or its choice) that the unit does not know yet;
- a **stat** (+amount HP, attack, defence, speed or move; HP raises the maximum and the current);
- an **upgrade** of one known, non-passive ability: +`levels.upgradePower` power or
  -`levels.upgradeCost` HP cost, kept per unit and slot (`Unit::boost`, `Unit::cut`). Every rule
  reads power and cost through `PowerOf` and `CostOf`; `Damage` adds the attacker's boost.
`LevelUp` (a = the card) applies it; a further level waiting is dealt next. Level-1 rows of
`jobs.csv` are still the starting abilities. Bots favour a new ability, attack and power.

## Range and heals (2026-11-20, the user's notes)

An ability's `range` is counted by its `reach` column: `walk` (steps through doors, `Steps`) or
`straight` (squares counted straight, walls ignored: the Manhattan distance to the nearest square
the target covers). The shipped data makes the ranged hits (Snipe, Dart, Throw) straight and the
rest walk. A heal on someone already at full HP is refused (`FullHp`), so bots and players never
waste one.

## Group battles

An attack takes in each side's group: the one and whoever of its side shares its tile (up to
two a group; `battle.fighters` by place, 0-1 the attacker's, 2-3 the defender's). Each round
every standing fighter picks once, in order: first strike first, then speed, the attackers first
on a tie (`battle.order`, `battle.next`). A strike names a standing fighter of the other group
(`BattleAbility` slot, target; no target: the first one standing); a shield is the picker's own
place's. A fighter downed drops out; when a whole group is down, or after `battle.rounds` rounds,
the battle ends (the round counter stops at the last). Monsters pick their ability as before and
strike the foe they can down, else the one with least HP (`MonsterTarget`).
