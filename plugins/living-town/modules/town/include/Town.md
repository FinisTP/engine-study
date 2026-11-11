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

## Memory and rumours (S22)

Notable things become **facts**: someone reached a level, was in a bad way, made a friend, fell
out, or the player kept or skipped a favour. The town keeps the latest 128.

Each fact has three sets of residents, one bit per resident (so a town has at most 64):

| Set | Meaning |
|---|---|
| `saw` | who was there: the people it is about, and everyone at the same place at that moment |
| `knows` | who knows by now: those who saw it, and those who were told |
| `told` | who has told the player |

**Rumours.** When two people talk (step 3 of the tick), each tells the other the newest fact the
other does not know yet. One fact each way per talk. That is the whole rule: news moves along
the same meetings that make friendships, so the people who drink together know each other's
business and someone who keeps to themselves hears little.

**What they think of the player** (`regard`, −1000..1000, per resident): learning that the player
kept a favour adds 80 for the one who asked and 40 for anyone else; a skipped one takes 120 and
60. It changes when the resident *learns* it, so a bad name spreads at the speed of talk.

## Favours

```json
"favours": [ { "id": "look_in_on_bren", "resident": "edda", "target": "bren", "hours": 24,
               "ask": "Look in on her for me before tomorrow night, would you?",
               "thanks": "Edda sent you? Tell her I'm well." } ]
```

`resident` asks the player to go and see `target` within `hours`. Talking to the asker starts it
(unless they think badly of the player); talking to the target in time keeps it; otherwise it is
skipped when the time is up, and at first only the asker knows.

## Talking (`TalkTo`)

The player talks to a resident and gets one line, in this order:

1. thanks, if the player was sent to them;
2. a favour, if they have one to ask;
3. the newest thing they know and have not told the player yet (news about the player first):
   in their own words if it is about them, "I saw it myself: ..." if they were there,
   "They say ..." if they only heard;
4. a greeting that shows their regard ("Good to see you!", "Hello.", "Oh. It's you.") and what
   they are doing.

These lines are plain templates. S23's model will word them; the templates stay as the fallback
for any line the model fails to give in time, and as what tests check.

## The chronicle

`Chronicle` lists the facts **someone has told the player** (and favours the player settled in
person), not everything that happened: the player learns the town by talking. `UmbraTool town-run
... --facts 1` prints every fact with how many residents know it.

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
- **Talking.** Near a resident the game shows "[E] Talk to ..."; **E** gives their line
  (`TalkTo`). **L** opens the chronicle screen (`plugins/living-town/assets/ui/chronicle.ui.json`).
- Screens get `town` in their data: `day`, `time`, `chronicle` (newest first), and for each
  resident `name`, `doing`, `place`, `money`, `level`.

## Try it

```
UmbraTool town-run projects/reference/assets/town/settlement.town.json --days 30 --seed 1
                   [--content projects/reference/assets/rpg/reference.rpg.json] [--json]
```

prints the notable events, where everyone ended up, the fingerprint and the speed (thousands of
days per second for seven residents).

## Limits (S21)

- Lines are templates unless a model file is present (S23, `../../mind/include/Mind.md`). No storyteller or story beats (S24).
- Residents do not walk between places: one who is shown vanishes when the town moves them.
- A resident tells one thing per talk and never repeats it; there is no conversation screen.
- Favours are of one kind (go and see someone) and each is asked once.
- Money is the only thing traded; there are no goods or stock.
- The example's balance (prices, wages, rise rates) is first-pass data, not tuned by play.
