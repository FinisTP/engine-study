# The storyteller and the outcome plan (S24)

ADR 0011: every playthrough must differ, and the developer manages how. The town's rules
(`../../town/include/Town.md`) say what *could* go wrong; the **outcome plan** is the developer's
file saying what the story must, may and must never do; the **storyteller** decides once a day
what actually happens, so that the plot arrives on time by whatever road this town took.

```
town rules:   Proposals(state) --> "Hale could steal from Edda", "Wynn could fall ill", ...
plan:         beats with windows, never, allow, pacing          (the developer writes this)
storyteller:  filter --> the beat first --> force on the last day --> else nudge by rehearsal
```

Code: `Plan.hpp` (the file), `Storyteller.hpp` (the daily step), `Soak.hpp` (many seeds).
None of it knows the world, the renderer or the model.

## The plan file (`town/<name>.plan.json`, beside `<name>.town.json`)

```json
{
  "format": "umbra.town.plan", "version": 1,
  "pacing": { "quietDays": 1, "maxIll": 2, "maxGone": 1, "lookAhead": 2, "hour": 12 },
  "never":  [ { "kind": "death", "who": "pip" }, { "kind": "leave", "who": "bren" } ],
  "allow":  [ { "kind": "leave", "who": "hale", "from": 6, "to": 12 } ],
  "beats":  [
    { "id": "missing", "name": "Something goes missing", "from": 2, "to": 4,
      "outcomes": [ { "kind": "theft" } ] },
    { "id": "reckoning", "name": "The reckoning", "from": 8, "to": 12,
      "outcomes": [ { "kind": "leave" }, { "kind": "death", "who": "bren" } ],
      "forbid":   [ { "kind": "ill", "who": "bren" } ] }
  ]
}
```

A **pattern** is a kind of trouble (`ill`, `theft`, `feud`, `leave`, `death`) with or without
names: `who` (who falls ill, steals, leaves, dies) and, for theft and feud, `other` (from whom,
with whom). A name left out means anyone. A feud has no first and second.

| Part | Meaning |
|---|---|
| `beats` | The main plot, in order. A beat opens when the one before it is reached. Between day `from` and day `to`, one of its `outcomes` must happen: that is the allowed variation. |
| beat `forbid` | Must not happen while this beat is the next one. |
| `never` | Must not happen in any playthrough. |
| `allow` | Who may leave or die outside the beats, and in which days. **Nobody leaves or dies unless a beat's outcome or `allow` says so.** |
| `pacing.quietDays` | Days of peace after any trouble (the plot may break this, see below). |
| `pacing.maxIll`, `maxGone` | People ill at the same time; people the town may lose in one playthrough. |
| `pacing.lookAhead` | Days the storyteller rehearses ahead when it nudges (0 = never). |
| `pacing.hour` | The hour of the day it acts (noon: people are about, so things are seen). |

Checked when loaded, before any town runs (`plan.*`): unknown kinds and residents, windows the
wrong way round, a beat whose window ends before the previous one can start (`plan.order`), a
beat with no outcome left after `never` and its own `forbid` (`plan.dead`), more beats that cost
the town someone than `maxGone` (`plan.gone`).

## The daily step (`StoryStep`)

Called after every tick; it acts when the clock reads the plan's hour.

1. **Missed.** A waiting beat whose window has closed is marked missed, with a warning.
2. **Filter** (`Permitted`). Of the town's proposals, those the plan permits today: possible; not
   in `never` or the waiting beat's `forbid`; not the waiting beat's own outcome before its window
   opens (too early would waste it); within `maxIll`; leaving and dying only by a beat or `allow`,
   within `maxGone`.
3. **The beat first.** If the waiting beat's window is open and a permitted proposal reaches it,
   that happens (one of them by dice): the beat **came by itself**.
4. **Force.** On the window's last day, if it still has not come: of *every* trouble that could
   be named and is permitted, the one the town has led up to most (`Likely`: the poorest thief,
   the worst-feeling pair, the weakest of the ill) is made to happen. The beat is **forced** and a
   warning says so. If nothing permitted is possible any more, the beat is **missed**.
5. **Quiet days.** Otherwise nothing happens for `quietDays` days after the last trouble.
   Steps 3 and 4 come before this on purpose: the plot outranks the quiet rule.
6. **Nudge.** With a beat waiting, each permitted proposal is rehearsed (below) against doing
   nothing; the best one happens if it beats doing nothing.
7. **A life of its own.** Otherwise, one day in three, one permitted proposal happens by dice.

The storyteller's dice come from the town's seed and clock, not from the town's own stream, so
the storyteller keeps no hidden state and one seed tells one story.

## Looking ahead (rehearsal)

The town's state is one copyable value (`Town.md` "forks"). To judge a proposal the storyteller
copies the town, lets the proposal happen in the copy, and lives the copy `lookAhead` days on
with the rules only:

```
score = 100 x (lookAhead - d + 1)   if on day d the copy's own proposals include a permitted
                                    outcome of the waiting beat (sooner is better), else 0
      -  10 x residents in a crisis at the end of the rehearsal
      -  30                         if it is the same kind of trouble as the last one
```

Example: the next beat needs a row. Rehearsing "Hale steals from Edda" shows Edda and Hale at the
inn two days later disliking each other: a row is proposed, score 100. Doing nothing: no row in
sight, score 0. So the theft happens today and the row comes by itself, instead of being forced
out of nowhere on the last day. At most eight proposals are rehearsed a day.

Measured on the settlement's placeholder plot (1,000 seeds): without rehearsal 19% of "bad blood"
and 29% of "reckoning" beats had to be forced; with two days of rehearsal 10% and 6%.

The rehearsal does not ask the language model (`Mind.md` "Forks do not ask the model"), so the
real town may go a little differently from the copy. The force on the last day covers that.

## Saves

`SaveStory`/`LoadStory`: which beats were reached, how, when and by what, and the pacing counters.
The game stores it inside the town's save (`"story"`). A save without one, or from a plan with a
different number of beats, starts the plot from its first beat. Warnings are not saved.

## The soak test

```
UmbraTool town-soak <town file> <plan file> [--seeds 1000] [--days <last window + 1>] [--json]
```

lives the plan on many seeds with nobody playing and prints, per beat, how often it came by
itself, was forced or was missed, on which days, and the spread of outcomes; then how many
different **roads** (the outcomes of all beats in order) the playthroughs took and how common
the commonest is. It counts troubles **against the plan** with its own reading of the plan, apart
from the storyteller's filter. Exit code 1 if anything was against the plan or a beat was missed.
A thousand playthroughs of 13 days take a few seconds.

Reading it: many forced beats mean the town rarely leads there (widen the window, add outcomes,
or change the town); one outcome at 90% means little variation; a missed beat is a plan mistake.

## In the game and the editor

- `apps/umbra/TownFlow` loads the plan beside the town file and calls `StoryStep` after every
  tick. Without a plan file the town is quiet (no trouble at all). Forced and missed beats go to
  the log. Residents who left or died are no longer shown; the ill stay at home.
- Trouble is remembered like any fact (`Town.md` "Trouble"), so residents talk about it and it
  reaches the chronicle through them.
- Editor, **Town plot** window (`plugins/living-town/editor/TownPlotPanel.cpp`): the beats with
  windows and outcomes, `never`, `allow`, pacing, the plan's mistakes, and a soak test at a
  button. The plan itself is edited as text.

## Limits (S24)

- Beats are reached by town trouble only. A beat cannot yet wait for something the player does,
  start a quest or change a scene: S25 connects beats to the game's quests.
- The player cannot cause or prevent trouble yet, except through favours and regard.
- Five kinds of trouble, fixed in code; a project cannot add its own kind in data yet.
- One plot line: beats are strictly in order, with no branches.
- The model does not propose trouble; proposals come from the rules.
- An owner who is gone leaves the place running with nobody paid (the coins leave the town).
