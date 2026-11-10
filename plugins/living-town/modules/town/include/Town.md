# The living town (S21)

A town is a handful of residents who get hungry, tired and lonely, have a home, a job, some money
and feelings about each other, and decide every few hours what to do next. It runs by itself,
whether or not the player is looking. This is the first part of the living-town plan
(`docs/adr/0011-living-town.md`); memory and rumours (S22), the built-in model (S23) and the
storyteller (S24) build on it.

Code: `TownDef.hpp` (the town as written, never changes), `Town.hpp` (the state and the rules),
`src/TownSave.cpp`. No renderer, physics, files or model: it is tested and fast-forwarded headless.

## The town file (`town/*.town.json`)

```json
{
  "format": "umbra.town", "version": 1, "urgent": 700,
  "needs":      [ { "id": "hunger", "name": "Hunger", "risePerDay": 1500 } ],
  "places":     [ { "id": "inn", "name": "the inn", "owner": "edda",
                    "scene": "scenes/settlement.scene.json", "marker": "town_inn" } ],
  "activities": [ { "id": "rest", "name": "resting at home", "where": "home", "relief": { "rest": 60 } },
                  { "id": "eat_inn", "name": "eating at the inn", "where": "inn", "hours": 1, "cost": 6,
                    "openFrom": 11, "openTo": 22, "social": true, "relief": { "hunger": 700 } } ],
  "residents":  [ { "id": "tobin", "name": "Tobin", "actor": "resident_tobin", "home": "house_row",
                    "work": "shop", "money": 90, "bonds": { "edda": 250 },
                    "schedule": [ { "from": 22, "to": 6, "activity": "sleep" } ] } ]
}
```

| Part | Fields |
|---|---|
| need | `id`, `name`, `risePerDay`: how much it grows in a day if nothing is done (a need runs 0..1000) |
| place | `id`, `name`; `owner` (the resident paid for what is bought here); `scene` + `marker` (where it is in the game world; without them the place is off screen) |
| activity | `id`, `name`, `where` (`home`, `work` or a place id), `hours`, `openFrom`/`openTo` (hours of the day; from > to wraps midnight), `relief` (per need, per hour), `cost` and `wage` (coins per hour), `xp` per hour, `social` |
| resident | `id`, `name`, `actor` (their RPG actor: stats, skills, look), `home`, `work`, `money`, `schedule` (first entry matching the hour is the plan), `bonds` (starting feelings, −1000..1000) |

The first activity must be free, at home and always open: it is what anyone can always do.
Nothing says "fantasy": a modern town has other needs, places and activities, the same rules.
Mistakes are reported by field (`town.where`, `town.schedule`, `town.actor`, ...).

## Time

One **tick** is 10 game minutes: 6 an hour, 144 a day. The state counts ticks from day 1, 00:00.

## One tick

1. **Whoever is free decides**, in resident order. A decision is an *intent* ("this resident starts
   this activity"). `Legal` checks it: the activity is open at this hour, the resident is not busy
   and can pay for the whole stay. `Apply` then moves them there, takes the cost (to the place's
   owner, or out of the town if nobody owns it), pays the wage and adds the experience. Nothing
   else moves a resident: the rule-based chooser, a test and (S23) the model all use this door.
2. **Needs change.** Every need grows; what the resident is doing eases some.
3. **On the hour, people talk.** Everyone doing something `social` picks one other person doing
   the same in the same place; both like each other a little more (one talk in six goes badly).

### The rule-based choice (`Choose`)

1. The plan: the first schedule entry for this hour that is legal now, else rest at home.
2. The most pressing need, judged twice: as it is now, and as it would be when the plan ends.
   If it is above `urgent` now, or would reach the limit by then, the resident deals with it first:
   the legal activity with the best `relief * 100 / (cost + 10)`. (So nobody goes to bed for
   seven hours on an empty stomach, and the poor forage instead of eating at the inn.)
3. Otherwise the plan.

An activity ends at its closing time if that comes before its hours are up (sleep that starts at
04:00 ends at 06:00). Started hours are paid in full.

## Whole numbers only

The same seed must give the same town on every compiler and machine, so the rules use no floating
point. The one awkward spot is a need that grows 1500 a day: that is 10.416 a tick. So needs are
stored **times 144**: a tick adds exactly `risePerDay`, and an hour's relief `r` takes off
`r * 24` a tick (`r * 144 / 6`). The limit is `1000 * 144`; `TownState::Need` divides again.

Chance (ties between equally good activities, who talks to whom, a talk going badly) comes from
the town's own generator, kept in the state: xorshift64* (three shifts and a multiply on one
64-bit number). Nothing reads the clock or `rand()`.

Test T90 holds a "golden" fingerprint of the reference town after one week; GCC, Clang and MSVC
must all produce it.

## One copyable block (forks)

`TownState` is plain data: a few numbers, an array of residents (52 bytes each), a table of bonds
and the recent events. **Copying it forks the world**:

```cpp
TownState rehearsal = town;          // a copy
Run(def, rehearsal, 2 * ticksPerDay); // two days ahead; the real town has not moved
```

A fork run forward gives exactly what the real town will give if nothing else interferes (T90).
S24's storyteller uses this to try a nudge in several copies and keep the best. The rule that
makes it work: **everything that changes lives in `TownState`**. No town data in scene objects,
statics or files.

## Events

The state keeps the latest 512 events: started an activity, paid, earned, reached a level, a need
at its limit, became friends (a bond crossing +300) or fell out (−300). `UmbraTool town-run`
prints the notable ones as sentences. S22 turns this into memory, rumours and the chronicle.

## Growth

Activities give experience; 100 make a level. The level and the resident's `actor` are what a
later phase needs to put the resident into a fight or the player's company as a full RPG
character. S21 itself only counts.

## Saves

`SaveTown` writes ids, not positions in lists, and the 64-bit seed and generator as text (JSON
numbers keep only 53 bits). `LoadTownState` refuses a save from a town file whose needs, places,
activities or residents changed (`town.state.town`) or a damaged one, and then changes nothing.
In the game the town travels in the player's save under `extras.town` (`SaveGame.md`); a save
without one starts a new town.

## In the game (`apps/umbra/TownFlow`)

- The project's town is the first `assets/town/*.town.json`.
- **One clock.** The town lives as far as the world's time of day moved since the last frame
  (S6's clock), so the sun and the schedules agree. A new game starts at dusk: the town is run
  from 00:00 up to that hour first. A jump of more than an hour (an area with a fixed hour, a
  loaded game) is not time passing.
- **Bodies.** A resident whose place has a `scene` and `marker` in the area the player is in gets
  a body there (their actor's model, wandering a step or two, peaceful). When the town moves them
  they vanish and appear at the new place: S21 does not walk them along the streets. Residents at
  places without a marker, or in other areas, live on unseen. Walking in and out shows them
  exactly where the town has them (the hand-over).
- Screens get `town` in their data: `day`, `time`, and for each resident `name`, `doing`, `place`,
  `money`, `level`.

## Try it

```
UmbraTool town-run projects/reference/assets/town/settlement.town.json --days 30 --seed 1
                   [--content projects/reference/assets/rpg/reference.rpg.json] [--json]
```

prints the notable events, where everyone ended up, the fingerprint and the speed (thousands of
days per second for seven residents).

## Limits (S21)

- No memory, rumours or chronicle (S22), no model (S23), no storyteller or story beats (S24).
- Residents cannot be talked to as residents yet, do not walk between places, and one who is
  shown vanishes when the town moves them.
- Money is the only thing traded; there are no goods or stock.
- The example's balance (prices, wages, rise rates) is first-pass data, not tuned by play.
