# The Last Curtain, plan 2b: goals and a bigger board (the rules) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace omens with secret personal goals that decide who turns at midnight, grow the board to 9×9 with 70 rooms and seven scattered start rooms, add the undo step, bot-to-bot chats and groups, goal-driven bots, and re-balance it all in the soak.

**Architecture:** Everything stays in the headless night (`plugins/last-curtain/modules/night`, `nightbots`): new data (`goals.csv`, keepsake columns in `items.csv`, new rooms and events), new state in the one copyable `NightState`, rules in `Turn.cpp`/`Company.cpp`/`Hunt.cpp`/`Board.cpp`, bots in `NightBots.cpp`, measurement in `NightSoak.cpp`. `LastCurtain.exe` (`apps/LastCurtain`) is only kept building and playable; its new screens are plan 2c.

**Tech Stack:** C++20, CMake, Catch2, nlohmann::json; data in CSV/JSON under `plugins/last-curtain/assets/night/`.

**Spec:** `docs/design/2026-11-14-goals-and-bigger-board.md` (approved 2026-11-14). Rules of the night: `modules/night/include/Night.md`; bots: `modules/nightbots/include/Balance.md`.

## Global Constraints

- Short, simple code with `// 1. ...` step comments; concepts in the module's `.md` (CLAUDE.md code style). C++ string/char literals ASCII. No identifiers `near`, `far`, `time`, `close`.
- All gameplay numbers in `assets/night/` (JSON/CSV); balancing never needs code. `[call]` marks Claude's fill-ins in data comments and docs.
- Use our own terms; no *Betrayal at House on the Hill* text or names; the Saigon Opera House keeps its real name.
- `NightState` stays copyable: no pointers, strings or maps; fixed-size arrays or `std::bitset`.
- A rules or data change updates the golden fingerprint in `tests/SaveTests.cpp` in the same commit.
- Commits: `issue #UMBRA-<n> <summary>` ending with the two attribution lines; stage files by name, never the user's `projects/reference/assets/scenes/settlement.scene.json`.
- Every task ends green on the dev harness (`/home/claude/dev`, `b/tests/UmbraNightTests`); Tasks 1, 2, 8 and 9 also run the full build (`/home/claude/build.sh gcc`), Task 9 runs `verify.sh` (gcc, clang, asan).
- Bands (spec): each Taken form 40–60%; each guest the Taken in 5–30% of nights; each goal held by a bot finished in 45–65% of the nights it is drawn; all-finished nights ≤ 10%; 45–65 rooms open by midnight; ≥ 1 bot-to-bot chat a round in ≥ 70% of evening rounds.

## Review Focus

1. A keepsake's holder dies or becomes the Taken before midnight: the keepsake drops in that room and another guest can still finish with it (test in Task 3).
2. An ally goal that names the player's guest: the bot must try to win the player over (it opens a chat with the player), never stall (test in Task 7).
3. A save made mid-evening keeps goals, known goals, groups, pairwise approvals and undo state: Continue gives the same night (test in Task 9).
4. A foyer with only three doors and a board edge close by: the seven start rooms are still all placed and connected, on 500 seeds (test in Task 4).
5. An event that pushes a guest back (`back`): that move cannot then be undone for a refund (test in Task 5).

---

### Task 1: Omens out (#137)

**Files:**
- Modify: `modules/night/include/{Night.hpp,NightDef.hpp,Night.md}`, `modules/night/src/{Company.cpp,Turn.cpp,Hunt.cpp,NightData.cpp,NightSave.cpp}`, `modules/nightbots/src/{NightBots.cpp,NightSoak.cpp}`, `modules/nightbots/include/{NightSoak.hpp,Balance.md}`, `tool/NightCommand.cpp`, `modules/board/src/Narrate.cpp`, `assets/night/{captions.json,rooms.csv,rules.json,styles.csv}`, `apps/LastCurtain/{Play.cpp,Play.hpp}`, `apps/LastCurtain/assets/ui/{sample.json}`; delete `assets/night/omens.csv`, `apps/LastCurtain/assets/ui/omen.ui.json`
- Modify docs: `docs/design/game-design.md` §1, §4, §6, §12, §13, §16 and the decision log (goals replace omens; text from the spec), `docs/design/cast.md` (each guest's goals, from the spec table)
- Test: `tests/{CompanyTests,HuntTests,TurnTests,SaveTests,BotTests,PlayTests,NightDataTests}.cpp`

**Interfaces:**
- Removes: `OmenDef`, `NightDef::omens`, `RoomKind::Omen`, `Rules::omens`, `StyleDef::omen`, `ActionKind::{TakeOmen, LeaveOmen}`, `LogKind::Omen`, `Cell::omen`, `Guest::omens`, `NightState::{omensTaken, omenOffer, omensUsed}`, `CurseTarget`, `inner::{OfferOmen, TakeOmen, TurnNow}`, `SoakReport::{cursed, cursedRounds, omens}`, soak strategies 2 and 3, Play's omen screen and data.
- Keeps: `NightState::marked` (set only at midnight from now on), `inner::TheTurn`.
- Produces: `TheTurn` with no omen pick: until Task 3, chance picks any living guest.

- [ ] **Step 1: Rewrite the tests that used omens.** Delete omen tests; change `HuntTests` "The Turn" cases to expect a random living guest marked at midnight; `NightDataTests` refuses a `rooms.csv` row of kind `omen` (`night.rooms`, unknown kind). The five omen rooms become `prompter,maskcabinet → item`, `office,vault → event`, `trapdoor → bonus` (stat/amount as the existing rooms of that kind) **[call]**.
- [ ] **Step 2: Run** `cmake --build /home/claude/dev/b && /home/claude/dev/b/tests/UmbraNightTests` — Expected: build errors or failures naming omens.
- [ ] **Step 3: Remove** everything listed under Removes; omen stat bonuses leave `Stat()`; `Play` drops the omen screen, its `omen` data and `take`/`leave` actions; captions lose `omenTake`/`omenLeave` (and `LoadCaptions` stops requiring them).
- [ ] **Step 4: Rewrite the docs** listed above from the spec (no omens; goals, the midnight draw, the board size), and log the decision with today's date.
- [ ] **Step 5: Run** the dev tests, then `/home/claude/build.sh gcc` — Expected: all pass; update the golden fingerprint.
- [ ] **Step 6: Commit** `issue #UMBRA-137 last-curtain: omens removed`.

### Task 2: A 9×9 board with 70 rooms (#138)

**Files:**
- Modify: `modules/night/include/Night.hpp` (`maxCells = 81`, `deck` size 128, `Guest::bonusUsed` → `std::bitset<maxCells>`), `modules/night/src/{Board.cpp,NightSave.cpp,Turn.cpp}`, `assets/night/{rules.json,rooms.csv,events.csv}`, `modules/board/src/BoardView.cpp` if it assumes 49
- Test: `tests/BoardTests.cpp`

**Interfaces:**
- Consumes: Task 1's room kinds.
- Produces: `rules.board = {width 9, height 9, rooms 70}`; deck of at least 85 entries.

- [ ] **Step 1: Write the failing tests** in `BoardTests.cpp`:
  - `"The deck fills a 70-room night (T116)"`: `state.deckSize >= 85`; on 500 seeds, opening every doorway until none is left places exactly `def.rules.board.rooms` rooms and the board never seals before that (reuse the existing seal test's loop).
  - `"Every event and trap room has an event to draw (T116)"`: count of deck entries of kind Event ≤ rows of `events.csv` with `trap=false`; same for Trap.
- [ ] **Step 2: Run** — Expected: FAIL (deck 30, rooms 20).
- [ ] **Step 3: Add data.** `rules.json` board 9×9, rooms 70. `rooms.csv`: keep the 28 rows; add rows (names and texts **[call]**, in the house's voice, ASCII) so the deck's counts are fillers/plain 28, item 16, event 18, trap 8, bonus 8, transport 2 (one pair, unchanged: `Pair()` assumes one pair) — fillers are kind `plain` with no stat (hallway, empty room, passageway, landing, cloakroom...). The spec's "second transport pair" is dropped: `Pair()` links the only two transport rooms, and a second pair would need pair ids for little gain (plan deviation, for the user's review). `events.csv`: six new events and three new traps **[call]** with `text`, so rows ≥ rooms of each kind. Widen `maxCells`, the deck and `bonusUsed` as listed; the save writes `bonusUsed` as a string of bits.
- [ ] **Step 4: Run** the dev tests and `night-soak --nights 300` (`/home/claude/soak.sh`) — Expected: tests pass; the soak runs with 0 faults (balance is Task 8's).
- [ ] **Step 5: Commit** `issue #UMBRA-138 last-curtain: a 9x9 board of 70 rooms`.

### Task 3: Goals and the midnight draw (#139)

**Files:**
- Create: `assets/night/goals.csv` (`guest,id,kind,target,count,text`)
- Modify: `modules/night/include/{NightDef.hpp,Night.hpp,Night.md}`, `modules/night/src/{NightData.cpp,Turn.cpp,Hunt.cpp,Company.cpp,Battle.cpp,NightSave.cpp}`, `assets/night/{items.csv,rules.json,captions.json}` (keepsake rows), `modules/board/src/Narrate.cpp`
- Test: create `tests/GoalTests.cpp`; add it to `tests/CMakeLists.txt`

**Interfaces:**
- Produces (NightDef.hpp): `enum class GoalKind : std::uint8_t { Room, Item, Ally, Events, Monsters };` `struct GoalDef { int guest = 0; std::string id; GoalKind kind = GoalKind::Room; int target = -1; int count = 1; std::string text; };` (`target`: rooms.csv, items.csv or guests.csv row; -1 for Events/Monsters) `NightDef::goals`; `ItemKind::Keepsake`; `ItemDef::room` (rooms.csv row where a keepsake waits, -1 otherwise); `Rules::goals = { int failHp = 1, earlyRooms = 35; }`.
- Produces (Night.hpp): `Guest::goal` (goals.csv row), `Guest::goalCount` (events passed or monsters destroyed so far, int8), `Guest::goalRoomSeen` (bool), `Guest::knows` (uint8, bits of guests whose goal this guest knows; its own bit set at the start); `bool GoalDone(const NightDef &, const NightState &, int guest)`; `LogKind::Goal` (who = guest, a = 1 done / 0 failed at midnight).
- Consumes: Task 2's board.

- [ ] **Step 1: Write the failing tests** in `GoalTests.cpp` (helpers: `Data()`, a `SetGoal(state, guest, "<goal id>")` that sets `Guest::goal` by id):
  - `"Goals load and bad goals are refused (T117)"`: every guest has 2–3 goals; a goal with an unknown room/item/guest, an unknown kind, a count < 1, or an item target that is not a keepsake is refused with `night.goals` and `goals.csv:<row>:<column>`.
  - `"Each guest draws one of its own goals, the same for the same seed (T117)"`.
  - `"Each goal kind is checked (T117)"`: room — entering the room sets it done, leaving keeps it done; item — done only while carried; ally — done only while the target is in the guest's group; events — done at `count` passed event rolls (traps do not count); monsters — done at `count` monsters destroyed by this guest (minions after midnight do not count).
  - `"A keepsake waits in its room only when a drawn goal needs it (T117)"`: opening that room puts it on the floor; with no goal needing it the room gives an ordinary item roll.
  - `"A dead holder drops the keepsake and another guest can finish with it (T117)"` (Review Focus 1).
  - `"At midnight the Taken comes from the failed (T117)"`: two failed guests → the marked one is one of them, the other has `min(hp, failHp)` HP; over 1,000 seeds each is drawn 40–60%; a downed failed guest's HP is untouched unless drawn.
  - `"If every living guest finished, the company wins at midnight (T117)"`: `state.over`, `Outcome::Company`, no Taken, `LogKind::Ended`.
  - `"A passed chat answer reveals the asker's goal (T117)"`: after the player passes one answer to guest g, `guests[player].knows` has bit g.
- [ ] **Step 2: Run** — Expected: FAIL (no `goals.csv`).
- [ ] **Step 3: Implement.** Loader for `goals.csv` and the `room` column of `items.csv` (five keepsakes from the spec: Phoenix crown → Costume store, Ledger → Shuttered office, Gilded mask → Mask cabinet, pocket watch → Box seats, ring of keys → Back corridor **[call]**). Goals from the spec table with the starting counts (Thanh events 2, Bao monsters 2, Khang monsters 3, Huy events 3) and texts **[call]**. `NewNight` draws each guest's goal with the night's RNG. Progress updates where rooms are entered, event rolls pass and monsters die. `TheTurn`: failed living guests → draw; none → company win; others `hp = min(hp, failHp)` unless down. Captions: `goalDone` ("{guest} has done what they came for.") for Narrate's result line.
- [ ] **Step 4: Run** the dev tests — Expected: PASS; update the golden fingerprint.
- [ ] **Step 5: Commit** `issue #UMBRA-139 last-curtain: personal goals decide who turns`.

### Task 4: Seven start rooms and early goal rooms (#140)

**Files:**
- Modify: `modules/night/src/{Board.cpp,Night.cpp}`, `modules/night/include/Night.md`
- Test: `tests/BoardTests.cpp`

**Interfaces:**
- Consumes: Task 3's drawn goals (draw goals before laying the board), `Rules::goals.earlyRooms`.
- Produces: `inner::BuildBoard(def, state)` lays the foyer (centre of the bottom row, as now) and one start room per guest; each guest starts in its own start room with `cell` and `from` set to it; start rooms are `resolved` (nothing on first entry).

- [ ] **Step 1: Write the failing tests** in `BoardTests.cpp`:
  - `"Seven start rooms, one guest each, connected to the foyer (T118)"` — 500 seeds (Review Focus 4): seven distinct cells besides the foyer, every guest on its own, a path of passages from each to the foyer, `placed == 8`.
  - `"No start room is a goal or keepsake room (T118)"`.
  - `"Goal and keepsake rooms come out within the first earlyRooms rooms (T118)"` — 500 seeds: open doorways until `placed == earlyRooms`; every room a drawn goal or keepsake needs is on the board.
  - `"A start room gives nothing on first entry (T118)"`: a start room of kind Item gives no item to its guest at the start.
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement.** After the shuffle, move the needed rooms to random places among deck positions `[7, earlyRooms - 1)`; start rooms take deck entries that are not needed rooms, each opened through a random doorway of the cells placed so far (the existing `OpenRoom`).
- [ ] **Step 4: Run** the dev tests — Expected: PASS; golden fingerprint updated.
- [ ] **Step 5: Commit** `issue #UMBRA-140 last-curtain: guests start scattered`.

### Task 5: The free step back, without pacing (#141)

**Files:**
- Modify: `modules/night/include/{Night.hpp,Night.md}`, `modules/night/src/Turn.cpp`
- Test: `tests/TurnTests.cpp`

**Interfaces:**
- Produces: `Guest::undo` (int8 cell the last plain step came from, -1 none) and `Guest::barred` (int8 cell left by an undo this turn, -1 none), both reset at `StartTurn`.

- [ ] **Step 1: Write the failing tests** in `TurnTests.cpp`:
  - `"Stepping back after a plain step gives the move back (T119)"`: A→B costs 1, B→A refunds it (`movesLeft` as before the first step).
  - `"A room left by an undo cannot be entered again that turn (T119)"`: after A→B→A, the Move toward B is not legal; next turn it is.
  - `"Only a plain step can be undone (T119)"`: a step that opened a room, rolled (event/trap), started a battle or was followed by a pick-up → stepping back costs a move.
  - `"A push back cannot be undone for a refund (T119)"` (Review Focus 5).
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement** in `Apply` for `Move`: a step into `undo` refunds and bars the cell left; any non-plain outcome clears `undo`.
- [ ] **Step 4: Run** — Expected: PASS; golden fingerprint updated.
- [ ] **Step 5: Commit** `issue #UMBRA-141 last-curtain: a free step back, no pacing`.

### Task 6: Chats between any two guests, and groups (#142)

**Files:**
- Modify: `modules/night/include/{Night.hpp,Night.md}`, `modules/night/src/{Company.cpp,Turn.cpp,NightSave.cpp}`, `modules/board/src/Narrate.cpp`, `assets/night/captions.json`, `apps/LastCurtain/Play.cpp` (chat data from the new fields)
- Test: `tests/CompanyTests.cpp`

**Interfaces:**
- Changes: `ChatState { bool active; std::int8_t asker, answerer, question; }` (drops `byBot`); `Guest::approvals` and `Guest::asked`/`chatRound` move to `NightState::approvals` (`std::array<std::int8_t, maxGuests * maxGuests>`, index `asker * maxGuests + answerer`: the asker's approvals of the answerer) — `asked` and `chatRound` stay per guest.
- Rule (the player's existing chats generalised): `Chat` (a = other guest) by guest I → the other guest O asks I one unasked question; I answers (`Answer`); a pass adds one approval from O to I and reveals O's goal to I (and I's to O when both are bots); at `persuade.approvals` O joins I's group. A bot's `OpenChat` toward the player stays as now (the bot asks, the player answers, the bot joins the player). Group numbers: 0 = the player's; a bot with no group that recruits gets group `1 + its index`. Groups have at most 3 members (as now for group 0).
- `Current()` returns `chat.answerer` while a chat is open.
- Captions: `chatBots` ("{guest} talks with {other}."), `joined` ("{other} joins {guest}.").

- [ ] **Step 1: Write the failing tests** in `CompanyTests.cpp`:
  - `"Two bots can chat; three passes bring the other into the opener's group (T120)"` — over separate turns.
  - `"Both bots learn each other's goal on a pass (T120)"`.
  - `"Groups hold at most three (T120)"`.
  - The existing player chat tests still pass with the new fields.
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement** as the Interfaces say; legal `Chat` for any guest (not only the player), same room, before midnight, the other not in its group, once per round per opener.
- [ ] **Step 4: Run** — Expected: PASS; golden fingerprint updated.
- [ ] **Step 5: Commit** `issue #UMBRA-142 last-curtain: guests talk to each other and form groups`.

### Task 7: Bots with a purpose (#143)

**Files:**
- Modify: `modules/nightbots/src/NightBots.cpp`, `modules/nightbots/include/Balance.md`, `assets/night/styles.csv` (`greed` column: 0 heal and keepsakes only … 10 everything **[call]**)
- Test: `tests/BotTests.cpp`

**Interfaces:**
- Consumes: Tasks 3–6 (goals, known goals, start rooms, undo, chats, groups).
- Produces: in the evening a bot picks one target at the start of its turn and walks a shortest path, in this order: its own goal (the goal room if open — opened rooms are public; a keepsake's room if open, or its holder only while that holder is lit for the bot; the ally-goal guest — a `Chat`, or `OpenChat` if it is the player; the nearest known unresolved event room; a seen monster it can beat), then a group member's known goal, then its style's wants, then the nearest doorway. It never steps back the way it came in a turn (no undo use), picks up only what `greed` allows, and opens chats with `chatty`.

- [ ] **Step 1: Write the failing tests** in `BotTests.cpp`:
  - `"A bot never steps back the way it came in a turn (T121)"` — 200 nights, every bot move: the new cell ≠ the cell it left this turn.
  - `"A bot walks toward its known goal room (T121)"`: a guest whose goal room is on the board and lit for it ends its turn nearer (path steps) than it started, or in it.
  - `"A bot with greed 0 leaves a worn item on the floor (T121)"`.
  - `"A bot whose ally goal is the player opens a chat with the player (T121)"` (Review Focus 2).
  - `"A bot helps a group member's known goal (T121)"`: with no goal work of its own left, it heads for the member's keepsake.
  - `"A bot does not chase a keepsake holder it cannot see (T121)"`: the holder in an unlit room is not its target (spec: bots act only on what their guest knows).
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement** in `NightBots.cpp` (evening part of `Choose()`; battles and the hunt keep their code); the target and path live in a small helper `Plan` inside the bot, recomputed each action from the state (no state stored).
- [ ] **Step 4: Run** the dev tests — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-143 last-curtain: bots with a purpose`.

### Task 8: The soak measures goals; balance (#144)

**Files:**
- Modify: `modules/nightbots/include/{NightSoak.hpp,Balance.md}`, `modules/nightbots/src/NightSoak.cpp`, `tool/NightCommand.cpp`, data in `assets/night/` (goal counts, Taken stats, monsters, `persuade`, `chatty`, `greed`) as the soak needs
- Test: `tests/BotTests.cpp` (`[balance]` case, CTest `Umbra.Night.Balance`)

**Interfaces:**
- Produces: `SoakReport` gains `goalsDrawn`/`goalsDone` (per goals.csv row, vectors), `takenBy` (per guest), `allFinished`, `roomsByMidnight` (sum), `eveningRounds`, `roundsWithBotChat`; `Holds()` checks every band in Global Constraints (goal bands only for goals drawn ≥ 50 times); `Text()` prints them.

- [ ] **Step 1: Write the failing test**: the `[balance]` case asserts `Holds()` on 1,000 nights from seed 1.
- [ ] **Step 2: Run** — Expected: FAIL (new bands unmeasured or out).
- [ ] **Step 3: Implement** the report, then tune data only until it holds on seeds 1, 20001 and 80001 (2,000 nights each); a goal that cannot be brought into 45–65% is replaced by another of the same guest **[call]**. Record each round of tuning in `Balance.md`.
- [ ] **Step 4: Run** `/home/claude/soak.sh` on the three seed ranges and the dev tests — Expected: `the data holds` each time.
- [ ] **Step 5: Commit** `issue #UMBRA-144 last-curtain: goals balanced`.

### Task 9: Saves, the table still plays, records (#145)

**Files:**
- Modify: `modules/night/src/NightSave.cpp` (version 2; every new field; checks ranges), `apps/LastCurtain/{Play.cpp,Main.cpp}` (shows the player's goal line in the HUD data as `goal` and `knownGoals`, chats between bots as captions; nothing else of 2c), `apps/LastCurtain/assets/ui/{hud.ui.json,sample.json}`
- Create: `docs/testing/s27-goals-and-board.md`; update `docs/ROADMAP.md`, `docs/current-engine-status.md`, `docs/design/HANDOFF.md`, `CLAUDE.md`
- Test: `tests/SaveTests.cpp`, `tests/PlayTests.cpp`

- [ ] **Step 1: Write the failing tests**: `"A mid-evening save round-trips goals, groups, approvals and undo (T122)"` (Review Focus 3); `"A version-1 save is refused (T122)"`; `PlayTests` `"The HUD data carries the player's goal (T122)"`.
- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement**; `ui-preview hud` with `sample.json` shows 0 errors.
- [ ] **Step 4: Run** `/home/claude/verify.sh` (gcc, clang, asan) and a SwiftShader capture of `LastCurtain --seed 3 --player nhi --frames 30` — Expected: all CTests pass; the capture shows the 9×9 board and scattered pawns.
- [ ] **Step 5: Write the records** (ROADMAP §14 template; soak numbers; the Windows check: MSVC build, `night-soak`, a night played).
- [ ] **Step 6: Commit** `issue #UMBRA-145 last-curtain: plan 2b records`, ship, then the final whole-branch review.
