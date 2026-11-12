# S24 — Storyteller & outcome plan: verification record

## Record

- Date: 2026-11-12
- Commit: `issue #UMBRA-101`, ADR 0011
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, ASan/UBSan). MSVC has
  not built this yet.
- State: **implemented**; the user's check on Windows (§4) is open.
- User decisions (2026-11-12): town events may be as dark as death, with the plan saying who may
  die or leave and when; the storyteller nudges first and forces a beat on its last day, with a
  warning; a placeholder three-beat plot for testing (S25 replaces it); residents get an optional
  `pronoun` in the town file.

## What was built

| Part | Where |
|---|---|
| Trouble: ill, theft, feud, leave, death; what the town proposes, what each does; residents who are gone | `plugins/living-town/modules/town` (`Town.hpp`, `src/Trouble.cpp`); `Town.md` "Trouble" |
| Facts and lines for trouble (residents talk about it; it reaches the chronicle) | `Sentence`, `Says` in `src/Town.cpp` |
| `pronoun` per resident; used by templates, the model's prompt (`About:` line) and a tenth guard check | `TownDef`, `TownMind.cpp`, `LineGuard.cpp`; `Mind.md` |
| The outcome plan file and its checks | `modules/story` (`Plan.hpp`); `Storyteller.md` "The plan file" |
| The storyteller: filter, the beat first, force on the last day, nudge by rehearsing forked towns | `Storyteller.hpp`; `Storyteller.md` "The daily step", "Looking ahead" |
| The soak test and its report | `Soak.hpp`; `UmbraTool town-soak`; `town-run --plan` |
| Game: the plan beside the town file is loaded, the story is in the save, the gone are not shown | `apps/umbra/TownFlow` |
| Editor: **Town plot** window (beats, rules, pacing, mistakes, soak at a button) | `plugins/living-town/editor/TownPlotPanel.cpp` |
| Example | `projects/reference/assets/town/settlement.plan.json`: "Something goes missing" (a theft, days 2-4), "Bad blood" (a row, days 4-8), "The reckoning" (someone leaves, or Old Bren dies, days 8-12); Pip never dies or leaves, Bren and Edda never leave |

## 1. Automated checks (T93, in `Umbra.Town`)

| Check | Result |
|---|---|
| Linux GCC / Clang / GCC ASan+UBSan | **53/53 / 53/53 / 53/53** |
| Each trouble does what it says; the impossible is refused; coins add up; the gone are out of every rule; through a save | pass |
| Ten plan mistakes are found by code before any town runs | pass |
| The filter: never, a beat's forbid, too early, over budget, unallowed leaving and dying | pass |
| A beat nothing leads to is forced on its last day with the expected warning; an impossible one is missed | pass |
| Same seed, same story, also through a save half way | pass |
| 1,000 playthroughs: plot always finished, nothing against the plan, every beat reached at least 3 ways and mostly by itself, at least 20 roads | pass |
| Rehearsal earns its keep: fewer forced beats with it than without (300 seeds each) | pass |
| Guard: wrong "he"/"she" refused, right one and "their" pass | pass (T92 extended) |

The golden fingerprint of seed 1, 7 days changed (the state gained `gone` and `illUntil`):
now `11171925813610364305`.

## 2. The soak report (cloud, debug build, 1,000 seeds, 13 days, 3.4 s)

```
1000 playthroughs of 13 days
plot finished: 1000 (100%)   against the plan: 0   troubles per playthrough: 5.21

beat "missing" (days 2-4): by itself 96%, forced 4%, missed 0; reached on days 2-4; 6 different outcomes
  43%  Pip stole coins from Old Bren
  35%  Hale stole coins from Edda
  11%  Hale stole coins from Wynn
  5%  Pip stole coins from Wynn
  4%  Hale stole coins from Old Bren
  3%  Pip stole coins from Edda

beat "bad_blood" (days 4-8): by itself 91%, forced 10%, missed 0; reached on days 4-8; 10 different outcomes
  31%  Wynn and Hale had a row in front of everyone
  23%  Edda and Hale had a row in front of everyone
  17%  Old Bren and Pip had a row in front of everyone
  9%  Old Bren and Garrick had a row in front of everyone
  5%  Wynn and Pip had a row in front of everyone
  5%  Old Bren and Edda had a row in front of everyone
  4%  Old Bren and Wynn had a row in front of everyone
  3%  Edda and Wynn had a row in front of everyone
  ... and 2 more

beat "reckoning" (days 8-12): by itself 94%, forced 6%, missed 0; reached on days 8-12; 4 different outcomes
  62%  Wynn has left town for good
  23%  Hale has left town for good
  8%  Garrick has left town for good
  6%  Old Bren has died

different roads through the plot: 69 (the most common one: 15% of playthroughs)
the plan holds (3448 ms)
```

Without rehearsal (`"lookAhead": 0`): "bad_blood" forced 19%, "reckoning" forced 29%.

## 3. One playthrough (`town-run --plan`, seed 5)

```
day 1 12:00  Garrick has taken ill
day 2 12:00  Hale stole coins from Edda
day 4 12:00  Wynn and Hale had a row in front of everyone
day 7 12:00  Edda and Hale had a row in front of everyone
day 8 12:00  Wynn has left town for good
day 12 12:00  Hale stole coins from Edda
beat "missing": came by itself, day 2 12:00: Hale stole coins from Edda
beat "bad_blood": came by itself, day 4 12:00: Wynn and Hale had a row in front of everyone
beat "reckoning": came by itself, day 8 12:00: Wynn has left town for good
```

## 4. Windows checklist (user)

1. Build (`windows-msvc-release`, or "Release: build, test, update known-good"). `Umbra.Town`
   must pass: it now includes T93 and the new golden fingerprint, which proves MSVC computes the
   same town.
2. The soak test, from `E:\Projects\EngineStudy`:
   `.\out\build\windows-msvc\tools\umbratool\Release\UmbraTool.exe town-soak projects\reference\assets\town\settlement.town.json projects\reference\assets\town\settlement.plan.json`
   The numbers must equal §2 exactly (same seeds, whole-number rules) and end with "the plan holds".
3. One story: `... UmbraTool.exe town-run projects\reference\assets\town\settlement.town.json --plan projects\reference\assets\town\settlement.plan.json --days 13 --seed 5` must print §3.
4. Editor: open the **Town plot** window: three beats, the four "never" lines, pacing; press
   **Run** under "Soak test". (The window was compiled in the cloud but never seen.)
5. Game: play some days in the settlement (L opens the chronicle). Residents should start telling
   of thefts, rows and illness; someone who left is no longer at their place. `umbra.log` has a
   line "Living town: day N: beat ..." whenever a beat was forced or missed.
6. With the model (`town-mind` as in S23): lines should now say "his"/"her" correctly; a wrong
   one shows as `REFUSED (says "he" of someone who is not)`.

## 5. Not verified, and limits

- Not built or run with MSVC. Not seen: the editor window, trouble in the running game.
- The placeholder plot is for testing the machine; it is not a story anyone chose (S25).
- Beats are reached only by town trouble; the player cannot yet cause or stop one, and a beat
  cannot yet start a quest (`Storyteller.md` "Limits").
- The trouble rules' numbers (10 coins is poor, two days ill) are first-pass data in code.
