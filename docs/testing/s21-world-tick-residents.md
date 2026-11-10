# S21 — World tick & residents: verification record

## Record

- Date: 2026-11-10
- Commit: `issue #UMBRA-91`, ADR 0011
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, ASan/UBSan); the game
  on SwiftShader (CPU Vulkan). MSVC has not built this yet: the golden fingerprint (§1) is the
  check that Windows gets the same town.
- State: **implemented**; the user's check on Windows (§4) is open.

## What was built

| Part | Where |
|---|---|
| New plugin `living-town` (needs `rpg`), module `Umbra::Town`: headless, integers only | `plugins/living-town/`; explained in `modules/town/include/Town.md` |
| Town files `town/*.town.json`: needs, places, activities, residents with schedules and bonds | `TownDef.hpp`, `src/TownDef.cpp` |
| The state as one copyable value; intents (`Legal`, `Apply`), the rule-based `Choose`, `Tick`, `Run`, `Fingerprint` | `Town.hpp`, `src/Town.cpp` |
| Saved town (ids, 64-bit numbers as text), refused when the town file changed | `src/TownSave.cpp` |
| `UmbraTool town-run`: live N days, print the notable events, the end state, the fingerprint and the speed | `plugins/living-town/tool/TownCommand.cpp` |
| Saves carry other plugins' parts (`extras`) | `plugins/rpg-session/modules/game` (`SaveData::extras`) |
| In the game: one clock with the world's time of day, residents shown at their place's marker, the town in saves and in the screens' data | `apps/umbra/TownFlow.hpp/.cpp`, `GameFlow.cpp`; peaceful roamers in `SceneRuntime` |
| Example | `town/settlement.town.json`: seven residents (Old Bren, Tobin the shopkeeper, Edda the innkeeper, Garrick the smith, Wynn and Hale in the fields, Pip the child), seven places (five with markers in the settlement), three needs, nine activities; seven `resident_*` actors in the RPG content |

## 1. Automated checks (T90)

| Check | Result |
|---|---|
| Linux GCC / Clang / GCC ASan+UBSan | **53/53 / 53/53 / 53/53** |
| The reference town loads against the RPG content: seven residents, owners, starting bonds | pass |
| Town file mistakes named by field (`town.format/needs/places/activities/where/relief/id/home/schedule/bonds/owner/actor`) | pass |
| Same seed, same week: two runs agree; another seed differs; the week's fingerprint equals the golden number on GCC, Clang and under ASan | pass |
| Fork: a copy run two days ahead leaves the original untouched; the original then reaches the same state; a nudged copy differs | pass |
| Intents: closed, unknown resident, unknown activity, busy and cannot-pay are refused and change nothing; a paid meal moves the coins to the owner; the owner eats free; every offered intent is accepted | pass |
| Thirty days in ten towns: places valid, money never negative, needs within 0..1000, coins conserved (residents' total = start + wages − costs to nobody) at every tick, schedules lived, fewer than 40 need crises, ten different endings | pass |
| Save: through text and back gives the same state and the same next week; a changed town file, a wrong place id, a missing field and another format are refused and change nothing | pass |
| A proposal for a free resident is used when legal; an illegal one leaves the town exactly as with nobody asking | pass |
| Saves: `extras` round-trips (64-bit numbers as text); older saves have none | pass |
| Fast-forward | 365 days of seven residents in 0.04 s in a debug build (about 9,000 days per second); the residents' state is 364 bytes |

## 2. What a week looks like

`UmbraTool town-run projects/reference/assets/town/settlement.town.json --days 30 --seed 1` prints
level-ups, the rare need crisis, and from the second week friendships ("Tobin now counts Edda as a
friend", "Hale now counts Garrick as a friend"); other seeds give other pairs on other days. Edda
(the inn's owner) grows rich, Hale drinks his wages and forages. It is alive but still quiet:
without memory, rumours and the storyteller nothing yet comes of these facts (S22-S24).

In the game (software rendering): residents stand at their places in the settlement.

## 3. Builds

| Build | Result |
|---|---|
| GCC Debug / Clang Debug / GCC ASan+UBSan | 53/53 each |
| Plugin sets without the town (real-time only; turn-only + learned AI) | 34/34, 38/38 |
| Web build (Emscripten, with the town) + `check-web.py` | built; **13/13** |

## 4. Windows checklist (user)

1. Build (the new test `Umbra.Town` must pass: it holds the golden fingerprint, so a pass means
   MSVC computes the same town as GCC and Clang).
2. `UmbraTool town-run projects\reference\assets\town\settlement.town.json --days 30 --seed 1`:
   the last line must show the same fingerprint on every run.
3. Start a new game. Around the settlement there are residents: at dusk some at the inn marker
   (south of the signpost) and at the well; Old Bren or Pip by the brick house.
4. Wait or walk around for a few minutes (a day is 12 minutes): people leave and appear at other
   places as the hours pass. Touching one starts no fight.
5. Save at the stone, quit, continue: the same people are where they were.
6. Go into the dungeon and come back after a while: the town moved on while you were away.

Known limits: residents vanish and appear instead of walking; they cannot be talked to yet; the
scene's Elder Mara (the quest giver) is not a town resident.
