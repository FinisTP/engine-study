# S22 — Memory, chronicle & rumours: verification record

## Record

- Date: 2026-11-11
- Commit: `issue #UMBRA-92`, ADR 0011
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, ASan/UBSan). MSVC has
  not built this yet; the golden fingerprint in `Umbra.Town` is the check that it computes the same.
- State: **implemented**; the user's check on Windows (§3) is open.

## What was built

| Part | Where |
|---|---|
| Facts with who saw, who knows and who told the player (one bit per resident); the latest 128 | `plugins/living-town/modules/town` (`Town.hpp`, `src/Town.cpp`); `Town.md` "Memory and rumours" |
| Rumours: on every talk each side passes on the newest fact the other lacks | `Tick` step 3 |
| What each resident thinks of the player (`regard`), changed when they learn of a kept or skipped favour | `Learn` |
| Favours in the town file (`favours`): asked by talking, kept by visiting the target in time, else skipped | `TownDef.hpp`, `TalkTo`, `Tick` step 4 |
| Template wording: a chronicle sentence and a spoken line for every kind of fact; greetings by regard | `Sentence`, `Says`, `TalkTo` |
| The chronicle: what the player has been told | `Chronicle` |
| Saved with the town; a save from S21 loads with empty memories | `src/TownSave.cpp` |
| Game: "[E] Talk to ..." near a resident, **E** talks, **L** opens the chronicle screen | `apps/umbra/TownFlow`, `GameFlow.cpp`, `Main.cpp`; `plugins/living-town/assets/ui/chronicle.ui.json`; `InputKey::L` |
| `UmbraTool town-run ... --facts 1` lists every fact and how many know it | `plugins/living-town/tool/TownCommand.cpp` |
| Example | two favours in the settlement: Edda asks the player to look in on Old Bren; Garrick sends word to Tobin |

## 1. Automated checks (T91, in `Umbra.Town`)

| Check | Result |
|---|---|
| Linux GCC / Clang / GCC ASan+UBSan | **53/53 / 53/53 / 53/53** |
| After 30 days: whoever a fact is about saw it; everyone who saw it knows it; facts are in time order; several have travelled beyond their witnesses; nobody told the player anything | pass |
| A town where nobody meets: every fact stays with its witnesses | pass |
| The roadmap's example: Edda asks, the player does nothing; after 25 hours the favour is skipped and only Edda knows (regard −120) and says so; two days later another resident has heard (regard −60) and says "They say you let Edda down: you never went to see Old Bren."; residents who have not heard are unchanged; the chronicle holds the line once | pass |
| A kept favour: the target thanks on the spot (regard +40), the chronicle has it, the deadline changes nothing, a week later the asker has heard (regard +80) and greets warmly | pass |
| Every kind of fact has a chronicle sentence, an own-words line, a "They say" line and an "I saw it myself" line | pass |
| Memories, regard and favours survive a save and continue identically; a save without them loads with none | pass |
| The week's golden fingerprint (now including memories) on GCC, Clang and ASan | pass |
| The chronicle screen drawn headlessly with `UmbraTool ui-preview chronicle` | looked at: title, day and time, lines, "The town now" list, Close |

## 2. Builds

| Build | Result |
|---|---|
| GCC Debug / Clang Debug / GCC ASan+UBSan | 53/53 each |
| Plugin sets without the town | 34/34, 38/38 |
| Web build + `check-web.py` | built; 13/13 |

## 3. Windows checklist (user)

1. Build; `Umbra.Town` must pass.
2. New game. Walk up to a resident: "[E] Talk to ..." appears. Press **E** several times: news,
   then a greeting with what they are doing.
3. Find Edda (the inn marker, south of the signpost; she is there most of the day): she asks you
   to look in on Old Bren. Press **L**: the chronicle is still empty or has what you were told.
4. Either go to Old Bren (the brick house, or the well in the morning) and press **E**: she
   thanks you and the chronicle says you kept your word. Or wait two game days (24 minutes) and
   talk to the people who drink at the inn in the evening: one of them will tell you they heard
   you let Edda down, and greets you coldly afterwards.
5. Save, quit, continue: the chronicle and what people think of you are kept.

Not checked in the cloud: how it plays (finding people, the pace of news), the hint and the
screen on a real display.
