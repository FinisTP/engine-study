# The Last Curtain: the battle royale — verification record

## Record

- Date: 2026-11-16
- Branch: `battle-royale` (the co-op game stays on `main`, the user's wish).
- Commits: `issue #UMBRA-217` (design and plan) to `#UMBRA-223` (balance and these records).
- Tester: Claude (remote session, overnight), Linux cloud workspace; one picture with SwiftShader; the
  killer's sound made on the user's PC.
- State: **implemented**; the Windows play check (§4) is open.
- Design: `docs/design/2026-11-16-battle-royale.md`; plan and ledger:
  `docs/design/plans/2026-11-16-battle-royale-plan.md`.

## What was built

| Stage | Items | Tests |
|---|---|---|
| 1 `#UMBRA-218` | The whole map at once (45 rooms), spawns far apart, fights between guests at any hour, death makes a ghost of the house (weak, respawns after 3 rounds), nights until one guest is left | T228-T234 |
| 2 `#UMBRA-219` | The killer (the Taken reused): wakes at midnight in the under-stage, hunts what it senses, kills the weakest in a room it enters, never destroyed; monsters bolder each hour | T236-T240 |
| 3 `#UMBRA-220` | Nine quests in `quests.csv` (carry the corpse from the wig room to the artists' waiting room, and so on), raced once a night, each giving a power and a stat; powers for guests | T241-T245 |
| 4 `#UMBRA-221` | Persuading with a request (spare me, an item, go after someone), then the other's question; truces; natures kind, dirty, wicked from `cast.md` | T246-T249 |
| 5 `#UMBRA-222` | The HUD's quest panel, power, night; ghosts drawn pale; the killer on the table and its warning and sound; every seen death staged on the fall screen; scenes for midnight, dawn and both endings | T250-T255 |
| 6 `#UMBRA-223` | Balance: the killer walks 1 room a round, +1 every 3 hours (was 2, +1 every 2); review: a ghost who strikes stands in the fall scene, the killer warning without a full UI rebuild | soak, T256 |

## 1. Automated checks

| Check | Result |
|---|---|
| Night suite (GCC) | 203/203 |
| `UmbraTool night-soak --runs 400` | the data holds (§3) |
| Full CTest under GCC, Clang, ASan + UBSan | 55/55 each (§1b) |
| `UmbraTool ui-preview hud` with sample data | 0 errors (warnings: no portrait files in the sample) |

Save version 6; the golden fingerprint changed with the rules (`tests/SaveTests.cpp`).

## 1b. Full runs

`/home/claude/verify.sh`: GCC 55/55, Clang 55/55; ASan found a heap overflow in `Pair` (a guest
standing on an unopened cell, in the transport test T95-era `TurnTests.cpp`): `Pair` now ignores
unopened cells. After the fix: GCC 55/55, Clang 55/55, ASan + UBSan 55/55, no sanitizer report.
`Umbra.Night.Balance` (the soak in CTest) passes.

## 2. Pictures (SwiftShader; not a GPU check)

A watched night at frame 200 (`LastCurtain --seed 3 --watch`): the board, a bot's room caption, the
sky. The killer and ghosts on the table were **not** captured (the killer wakes at round 7, too far
for a SwiftShader run); their look is part of the Windows check.

## 3. Soak

400 runs: 0 faults; 2.3 nights per run (band 2-4); per night 2.5 deaths (by guests 1.5, ghosts 0.2,
the house 0.7), 1.6 monster kills, 3.2 quests, 1.7 powers used. Winners: khang 29%, lan 27%, mai
13%, bao 12%, thanh 11%, nhi 3%, huy 2% (at most 35% each). The house won 0% (at most 30%).
Open for the user: the kind guests rarely win, and the house almost never does.

## 4. The user's check on Windows

On branch `battle-royale`:

1. A new night: the opening scene; guests spread far apart over a whole map.
2. The Quests panel (top left) lists the open quests with their rewards; finish one (the corpse is in
   the wig room): it shows "taken by you", your power shows with its cooldown.
3. Walk into another guest's room: the menu offers "Attack X", "Ask X to spare you", "Ask X for an
   item", "Ask X to go after Y". Bots attack each other and you.
4. A death you can see: the fall screen in that room says who fell and how; Continue goes on.
5. Round 7: the midnight scene; the killer (the Taken's figure, dark red on a red ring) when its
   room is lit; within two rooms the red warning and its dragging steps. It cannot be attacked.
6. Die: "YOU ARE A GHOST"; you play on from the under-stage, pale and glowing; you can attack the
   living.
7. Dawn with two or more alive: the dawn scene, a new map, night 2 in the top bar.
8. The end: one left alive walks out (`ending_escaped`), or none (`ending_house`).
