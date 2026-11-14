# The Last Curtain, plan 1: the night as a simulation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The whole night of *The Last Curtain* playable by bots with no graphics, driven entirely by data files, with a soak test that reports balance over 1,000 nights.

**Architecture:** A new source plugin `plugins/last-curtain` with two headless modules: `night` (data, one copyable `NightState`, the rules behind one `LegalActions`/`Apply` interface) and `nightbots` (the bots and the soak test). Every number lives in `plugins/last-curtain/assets/night/*.csv|json`; C++ holds only the mechanics. The 3D board, UI, model lines and art are later plans that drive the same `LegalActions`/`Apply`.

**Tech Stack:** C++20, CMake plugin system (`plugin.json`), nlohmann-json, Catch2, `UmbraTool` commands (`UMBRA_TOOL_COMMAND`).

**Spec:** `docs/design/game-design.md` (draft 7) and `docs/design/cast.md` (draft 5), approved by the user 2026-11-14. Section numbers below ("GDD 11") refer to the first.

## Scope

The spec covers four separable pieces. Each gets its own plan and each ends in something that runs:

| Plan | Delivers | State |
|---|---|---|
| **1. The night as a simulation** | Rules, data, bots, soak test, balance | this document |
| 2. The board on screen | 3D board, light and dark, camera on the acting guest, UI, the player's turn, allies, the battle screen | to be written after plan 1 |
| 3. The model's lines | Bot decisions, omen lines, the Turn's tale, the Taken's lines, through the existing guard | to be written |
| 4. Art | Room kit for the opera house, seven guests, seven Taken forms | to be written |

## Global Constraints

- Short, simple, straightforward code; step comments (`// 1. ...`); a markdown file beside the code for concepts (`Night.md`, `Balance.md`). (CLAUDE.md "Code style")
- **All gameplay data is metadata** (user, 2026-11-14): every stat, cost, chance, target, count and list is read from `assets/night/`. A number in C++ is a defect unless it is a mechanic's structure (grid directions, array capacities). A new *kind* of mechanic may need code; retuning never does.
- Rules are whole-number and deterministic: same data + same seed = same night on GCC, Clang and MSVC. No `float`, no `std::uniform_int_distribution` (use the xorshift pattern of `plugins/living-town/modules/town/src/Town.cpp`).
- All changing data is inside `NightState`, which is a plain copyable value (no pointers, no strings, no maps): bots copy it to look ahead.
- `night` and `nightbots` link no renderer, window, model, audio or network library, and do not depend on the `rpg` plugin.
- C++ string and char literals are ASCII. Do not name identifiers `near`, `far`, `time` or `close` (MSVC).
- Namespaces `umbra::night`; headers under `include/` included by bare name; libraries `Umbra::Night`, `Umbra::NightBots`; CTest name `Umbra.Night`.
- Commit messages: `issue #UMBRA-<n> <summary>`, numbering on from `#UMBRA-103` (the design and plan documents).
- The spec's terms: guests, the Taken, the company, the Turn, omen, marked, the under-stage. No wording, card names or scenario names from the board game's rulebook.
- Verification commands (cloud): `TAIL=6 /home/claude/build.sh gcc`, then `ctest --test-dir /home/claude/build-gcc -R Umbra.Night --output-on-failure`; before the last commit `bash /home/claude/verify.sh` (GCC, Clang, ASan). Windows: build preset `windows-msvc-release`, test preset `windows-msvc-release`.

**One departure from the spec, for the user to confirm:** GDD 10 says battles are "the engine's existing turn-based battles". The engine's rules have one Attack and one Defense stat, order by speed, no round limit, and state held in strings and maps. The night needs Might and Sanity as two damage types, the starter attacking first, a 3-round cap, and a state cheap to copy thousands of times. So this plan gives the night its own small battle rules (Task 5), data-driven like the rest. Plan 2 reuses the engine's battle *screen*, not its rules.

## Review Focus

Conditions the spec implies but does not spell out, most likely first; each is pinned by a test in the task named.

1. **A data file is missing, misspelt or holds a bad value** (a blank cell, text where a number goes, an unknown id): loading fails with a diagnostic naming file, row and column; nothing crashes; the tools exit 1. (Task 1)
2. **The room deck cannot continue the board** (every remaining tile would seal it, or the deck runs out): for every seed the board stays connected with an unopened doorway until all rooms are placed, and the under-stage is always placed. (Task 2)
3. **Nobody takes an omen, or the marked guest is the player, an ally, or already down at midnight:** a Taken always appears, the group loses a marked ally, and a marked guest felled by the house returns at full HP. (Tasks 7, 8)
4. **A power or action with no valid target** (line with nobody in it, swap with no opened room, seal that would cut the under-stage off from the whole company): `LegalActions` never offers it; a seal may never leave the Taken unreachable. (Task 9)
5. **A night saved in the middle of a battle or of the Turn** loads and continues to the same fingerprint. (Task 10)

---

## File Structure

```
plugins/last-curtain/
  plugin.json                       name "last-curtain", kind "game", modules night, nightbots
  assets/night/                     ALL gameplay data (Task 1 lists every file and column)
  modules/night/
    CMakeLists.txt
    include/NightDef.hpp            data structs, LoadNight
    include/Night.hpp               NightState, Action, NewNight, LegalActions, Apply, queries
    include/Night.md                the rules as built, file by file
    src/Csv.hpp, Csv.cpp            a 60-line CSV reader (quoted fields, header row)
    src/NightData.cpp               LoadNight and validation
    src/NightInner.hpp              helpers shared by the rule files (dice, Guest(), Log)
    src/Board.cpp                   deck, placement, doors, reach, light, line of sight
    src/Turn.cpp                    clock, order, movement, items, rooms, checks
    src/Battle.cpp                  battles, buffs, XP and levels, monsters
    src/Company.cpp                 guest against guest, persuading, bonds, omens and the mark
    src/Hunt.cpp                    the Turn, the Taken's turn, powers, the end
    src/NightSave.cpp               Fingerprint, SaveNight, LoadNightState
  modules/nightbots/
    CMakeLists.txt
    include/NightBots.hpp           ChooseAction, PlayNight
    include/NightSoak.hpp           Soak, report text and JSON
    include/Balance.md              how to tune: which file moves which number of the report
    src/NightBots.cpp, src/NightSoak.cpp
  tool/NightCommand.cpp             UmbraTool night-check, night-run, night-soak
  tests/CMakeLists.txt, tests/NightDataTests.cpp, BoardTests.cpp, TurnTests.cpp,
        BattleTests.cpp, CompanyTests.cpp, HuntTests.cpp, SaveTests.cpp, BotTests.cpp
projects/reference/project.json     add "last-curtain" to "plugins"
docs/adr/0012-last-curtain.md       the pivot (written with this plan)
```

Tests get the data folder as `UMBRA_NIGHT_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../assets/night"`. A helper in each test file: `NightDef Data()` loads it with `REQUIRE`. Tests that need a special situation copy the loaded `NightDef`, change a field, and build a state with `NewNight`; they never depend on a tuned number staying the same unless the task says "golden".

---

### Task 1: Plugin, data files and loading

**Files:**
- Create: `plugins/last-curtain/plugin.json`, `modules/night/CMakeLists.txt`, `include/NightDef.hpp`, `src/Csv.hpp`, `src/Csv.cpp`, `src/NightData.cpp`, `tests/CMakeLists.txt`, `tests/NightDataTests.cpp`, every file under `assets/night/`
- Modify: `projects/reference/project.json` (add `"last-curtain"` after `"living-town"`)

**Interfaces:**
- Produces: `struct NightDef` and `bool LoadNight(const std::filesystem::path &folder, NightDef &out, std::vector<core::Diagnostic> &diagnostics)`; diagnostics use code `night.<file>` and field `<file>:<row>:<column>`. One struct per file with one member per column, same name in camelCase; ids become indexes (`int`) after loading; `NightDef::Find(const std::vector<T>&, std::string_view id) -> int`.

The data files. Every row's `id` is lowercase ASCII; ids referenced from another file must exist.

| File | Columns (first values from the spec; `[call]` numbers are starting points for Task 12) |
|---|---|
| `rules.json` | `clock{startHour 19, rounds 20, turnAfterRound 10}`, `board{width 5, height 5, rooms 20}`, `checks{easy 10, normal 13, hard 16}`, `light{steps 4}`, `carry 4`, `battle{rounds 3, minDamage 1, base 3, defenseDivisor 2, guardDivisor 2}`, `levels{xp [2,5,9,14], hp 4, stat 1}`, `omens{targets [6,9,12,15,18]}`, `monsters{fromRound 3, chancePercent 35, stayRounds 3, max 2}`, `persuade{target 13, bond 3, retryRounds 2}`, `guests{downedReturnHp 1, attackBondLoss 300}`, `balance{companyWinMin 40, companyWinMax 60}` |
| `guests.csv` | `id,name,pronoun,weapon,hp,might,sanity,speed,style,temper,taken,takenHp,takenMight,takenSanity,takenSpeed,power` — the seven rows of `cast.md` (nhi 16/3/6/5 physical explorer; thanh 20/2/7/4 magical loner; bao 24/4/6/3 physical protector; khang 30/7/2/3 physical fighter; mai 18/2/8/3 magical schemer; huy 20/3/4/6 magical explorer; lan 22/5/5/4 physical schemer). `weapon` is `physical` or `magical`; `temper` is `fight` or `flee` (khang, nhi, lan, bao fight; thanh, mai, huy flee). Taken rows `[call]`: HP 90, the guest's Might and Sanity +4, Speed as the guest; `power` = `line, area, flood, pull, seal, swap, reflect` in cast order. |
| `bonds.csv` | `from,to,bond` — the cast's bonds as -1000..1000 (liked +300, disliked -300); pairs not listed are 0. |
| `powers.json` | one object per power id: `kind` (same seven words; the kind selects the code path), then that kind's numbers: `damage`, `range`, `rounds`, `cooldown`. First values `[call]`: line{damage 8}, area{damage 5, range 2}, flood{damage 3, rounds 2}, pull{damage 6, range 3}, seal{range 3, rounds 2}, swap{}, reflect{}; all `cooldown 1` (usable every turn). |
| `rooms.csv` | `id,name,kind,doors,stat,amount,count` — `kind` one of `foyer, understage, plain, item, event, trap, omen, transport, bonus`; `doors` a subset of `NESW` before rotation; counts 1,1,7,4,4,3,5,2,3 (GDD 6); bonus rows: rehearsal room might 1, ancestors' chapel sanity 1, rigging gallery speed 1. Give about a third of the rooms two doors, a third three, a third four `[call]`. |
| `events.csv` | `id,name,kind,stat,target,passEffect,passAmount,failEffect,failAmount` — `kind` `event` or `trap`; `stat` `might|sanity|speed`; `target` `easy|normal|hard`; effects from `none, hp, maxhp, might, sanity, speed, item, xp` (negative amounts hurt). 12 events and 6 traps `[call]`; a trap's `passEffect` is `none`. |
| `items.csv` | `id,name,kind,effect,amount,buff,weight,text` — `kind` `heal|boost|worn|relic`; `effect` `hp, hproom, buff, might, sanity, speed, maxhp, armor, light, revive`; the 12 items of GDD 9 plus `crucifix` and `rosary` (relic, revive, weight 1 against 6 for the rest `[call]`); `text` is the one-line UI explanation. |
| `buffs.csv` | `id,name,stat,amount,rounds` — guarded (guard, 0 = until hit), strengthened (might +2, 0 = the battle), focused (sanity +2, 0), weakened (might -2, 2), shaken (sanity -2, 2). |
| `monsters.csv` | `id,name,hp,might,sanity,attack,resist,xp,debuff,hpPerHour,statPerHour` — moth swarm, stagehand, understudy (GDD 10); `xp 1` each. |
| `omens.csv` | `id,name,might,sanity,speed,maxhp` — the five of GDD 12. |
| `styles.csv` | `style,explore,fight,loot,omen,bonus,fleeBelowHpPercent,avoidGuests` — one row per play style; weights 0..10 read by the bots (Task 11). |

- [ ] **Step 1: Write the failing tests** in `tests/NightDataTests.cpp`

```cpp
TEST_CASE("The shipped data loads and is complete (T94)", "[night][data]")
{
    std::vector<core::Diagnostic> diagnostics;
    NightDef def;
    REQUIRE(LoadNight(UMBRA_NIGHT_DATA, def, diagnostics));
    CHECK(def.guests.size() == 7);
    CHECK(def.powers.size() == 7);
    CHECK(def.omens.size() == 5);
    CHECK(def.rules.clock.rounds == 20);
    CHECK(def.rules.carry == 4);
    CHECK(def.guests[static_cast<std::size_t>(def.Find(def.guests, "khang"))].might == 7);
    int rooms = 0;
    for (const RoomDef &room : def.rooms) rooms += room.count;
    CHECK(rooms >= def.rules.board.rooms);
}
TEST_CASE("Bad data is refused with file, row and column (T94)", "[night][data]")
```

The second case copies the data folder to a temporary folder once per section, breaks one thing, and checks `LoadNight` is false and the first diagnostic's `code`/`field`: a deleted `items.csv` (`night.items`, field `items.csv`); `might` = `x` in `guests.csv` row 2 (`guests.csv:2:might`); a blank `hp`; `power` = `nosuch` (`guests.csv:1:power`); a `rooms.csv` with no `understage`; `rules.json` with `carry` 9 (capacity is 6); a CSV row with too few cells; a quoted field containing a comma loads correctly.

- [ ] **Step 2: Run** `ctest --test-dir /home/claude/build-gcc -R Umbra.Night --output-on-failure` — Expected: build fails, `NightDef.hpp` not found.
- [ ] **Step 3: Write `Csv.hpp/.cpp`:** `struct CsvTable { std::vector<std::string> header; std::vector<std::vector<std::string>> rows; int Column(std::string_view name) const; }` and `bool ReadCsv(const std::filesystem::path &file, CsvTable &out, std::string &error)`; handles `"quoted, fields"`, `""` inside quotes, CRLF, a trailing blank line.
- [ ] **Step 4: Write `NightDef.hpp` and `NightData.cpp`,** the CMake files (copy the shape of `plugins/living-town/modules/town/CMakeLists.txt` and `tests/CMakeLists.txt`), `plugin.json`, and all data files with the values above.
- [ ] **Step 5: Run the tests** — Expected: PASS; also `TAIL=6 /home/claude/build.sh gcc` shows every existing test still passing.
- [ ] **Step 6: Commit** `issue #UMBRA-104 last-curtain: plugin, data files and loader`

### Task 2: The board

**Files:** Create `include/Night.hpp`, `src/NightInner.hpp`, `src/Board.cpp`, `tests/BoardTests.cpp`

**Interfaces:**
- Produces (in `Night.hpp`):
  - `inline constexpr int maxGuests = 7, maxCells = 49, maxCarry = 6;`
  - `struct Cell { std::int8_t room = -1; std::uint8_t doors = 0; bool opened = false; };` (`room` indexes the night's shuffled deck entry's `RoomDef`; `doors` is the rotated NESW mask, bit 0 = N)
  - `struct NightState` with, so far: `std::uint64_t seed, random; std::array<Cell, maxCells> cells; std::array<std::int8_t, 64> deck; int deckNext;` (grows in later tasks)
  - `NightState NewNight(const NightDef &def, std::uint64_t seed);`
  - `int CellAt(const NightDef &def, int x, int y);` `bool Passage(const NightDef &def, const NightState &state, int from, int direction);` (both cells opened and both have the facing door)
  - `bool Doorway(const NightDef &def, const NightState &state, int from, int direction);` (a door facing an in-grid cell not yet opened)
  - `int Open(const NightDef &def, NightState &state, int from, int direction);` (draws, rotates and places the next room; returns the new cell)
  - `int Steps(const NightDef &def, const NightState &state, int from, int to);` (through passages; -1 if unreachable)
  - `bool Lit(const NightDef &def, const NightState &state, int viewer, int cell, int steps);`
  - `bool InLine(const NightDef &def, const NightState &state, int from, int to);` (a straight unbroken line of passages)

Placement rule (the algorithm the tests do not fully determine):

```
Open(from, direction):
  1. target = the cell beyond that doorway.
  2. Take deck entries from deckNext on; for each, try its four rotations and keep those that
     have a door facing `from`.
  3. Accept the first entry+rotation after which the board is not sealed: some opened cell
     still has a Doorway, or no rooms are left to place.
  4. Entries skipped stay in the deck in order. If none fits, place a plain room with four doors.
  5. The under-stage is forced as soon as the target is 4 or more steps from the foyer and it
     is not yet placed, and at the latest as the last room.
```

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("A new night has only the foyer, bottom centre, with a doorway (T95)", "[night][board]")
// cells opened == 1; its RoomDef kind == Foyer; at least one Doorway from it.
TEST_CASE("Opening every doorway builds a connected board on every seed (T95)", "[night][board]")
// for seed 1..500: repeatedly Open the first Doorway found until none is left;
// CHECK opened == def.rules.board.rooms; every opened cell has Steps(foyer, cell) >= 0;
// exactly one UnderStage, and Steps(foyer, understage) >= 3; room kind counts never exceed rooms.csv counts;
// while rooms remained, a Doorway always existed (never sealed).
TEST_CASE("Doors must face each other to pass (T95)", "[night][board]")
// build two cells by hand: A with door E, B with door N only -> Passage(A, E) false, Steps -1.
TEST_CASE("Light reaches by steps through doors, not through walls (T95)", "[night][board]")
// a hand-built corridor of 6 cells: Lit(viewer at 0, cell 4, 4) true, cell 5 false;
// a cell 1 apart on the grid but 7 steps round a wall is not lit.
TEST_CASE("A line is straight and unbroken (T95)", "[night][board]")
// same row with passages: InLine true; one missing door between: false; a corner: false.
TEST_CASE("The same seed builds the same board (T95)", "[night][board]")
```

- [ ] **Step 2: Run** — Expected: FAIL to build (`Night.hpp` missing).
- [ ] **Step 3: Implement** the interface above in `Board.cpp`; dice and small helpers in `NightInner.hpp` (`std::uint64_t Dice(NightState &state)`, `int Roll(NightState &state, int sides)`).
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-105 last-curtain: the board`

### Task 3: The clock, a turn, moving and items

**Files:** Create `src/Turn.cpp`, `tests/TurnTests.cpp`; modify `Night.hpp`

**Interfaces:**
- Produces:
  - `struct Guest { std::int16_t hp, maxHp, might, sanity, speed; std::int8_t cell, level, group = -1; std::int16_t xp; std::array<std::int8_t, maxCarry> items; /* -1 = empty */ std::uint8_t omens = 0, bonusUsed = 0; bool down = false, dead = false; std::int8_t movesLeft = 0; };`
  - `NightState` gains `std::array<Guest, maxGuests> guests; std::array<std::int8_t, maxGuests> order; std::int8_t round = 1, turn = 0; std::int8_t player = -1; std::vector<FloorItem> floor;` with `struct FloorItem { std::int8_t cell, item; };`
  - `NewNight(def, seed, int player = -1)`: every guest in the foyer at full HP; order = d20 each, high first, ties to higher Speed, then to guest index.
  - `enum class ActionKind : std::uint8_t { Move, UseItem, DropItem, PickUp, EndTurn, /* later tasks: */ TakeOmen, LeaveOmen, Persuade, Attack, Join, Strike, Guard, Leave, LevelUp, Power };`
  - `struct Action { ActionKind kind = ActionKind::EndTurn; std::int8_t a = -1, b = -1; bool operator==(const Action &) const = default; };` (`Move`: a = direction; item actions: a = slot or floor index; others documented where they are added)
  - `int Current(const NightState &state);` (the guest to act; -1 when the night is over)
  - `std::vector<Action> LegalActions(const NightDef &def, const NightState &state);`
  - `bool Apply(const NightDef &def, NightState &state, const Action &action);` (false and no change if not legal)
  - `int Hour(const NightDef &def, const NightState &state);` `int Minute(...)` (round 1 = 19:00, each round 30 minutes)
  - `int Stat(const NightDef &def, const NightState &state, int guest, StatKind kind);` with `enum class StatKind { Might, Sanity, Speed, MaxHp, Light }` (base + worn items + omens + buffs)

Rules pinned by the spec (GDD 5, 9): `movesLeft` starts at Speed; `Move` through a passage costs 1; `Move` through a doorway opens the room: a plain or already-resolved room does not stop movement (the under-stage counts as plain until the Turn), any other kind sets `movesLeft = 0` and (from Task 4) resolves it, which ends the turn; a transport room moves the guest to its pair for 1 move; ending the turn in an unused bonus room adds its stat once per guest. `UseItem` (heal kinds only on the board), `DropItem`, `PickUp` are free and legal at any point of the turn; a guest holds at most `rules.carry` items; a floor holds any number. After the last guest of a round, `round` increases.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("Order is a d20 each, ties to Speed, fixed for the night (T96)", "[night][turn]")
// seeds 1..50: order is a permutation of 0..6; replaying the seed gives the same order;
// with a NightDef whose guests all have equal dice forced by seed search is not needed:
// instead check the documented tie rule through inner::Order(rolls, speeds) directly.
TEST_CASE("A guest moves up to Speed and plain rooms do not stop it (T96)", "[night][turn]")
// a data copy whose deck is all plain 4-door rooms: the first guest can Apply Move four times
// if Speed is 4, then Move is no longer in LegalActions; EndTurn passes to order[1].
TEST_CASE("A room with something in it stops movement (T96)", "[night][turn]")
// deck all bonus rooms: after one Move through a doorway, movesLeft == 0 and Move is not legal.
TEST_CASE("A bonus room pays once per guest when the turn ends there (T96)", "[night][turn]")
TEST_CASE("Transport rooms are a pair (T96)", "[night][turn]")
TEST_CASE("Items: four carried, any number on a floor, all free (T96)", "[night][turn]")
// give a guest 4 items and a floor item: PickUp not legal; after DropItem it is, and Move is
// still legal afterwards; dancer's shoes raise Stat(Speed) by 1 while carried, not after dropping;
// a bandage heals 6 but not above Stat(MaxHp).
TEST_CASE("Twenty rounds run from 19:00 to 04:30 (T96)", "[night][turn]")
// every guest only EndTurn: after 7 Applies round == 2 and Hour/Minute == 19:30;
// after 140 Applies Current() == -1.
```

- [ ] **Step 2: Run** — Expected: FAIL to build.
- [ ] **Step 3: Implement** in `Turn.cpp`.
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-106 last-curtain: clock, turns, movement, items`

### Task 4: Checks, events, traps, item rooms, down and relics

**Files:** Modify `src/Turn.cpp`, `Night.hpp`, `tests/TurnTests.cpp`

**Interfaces:**
- Produces:
  - `struct Check { int die, stat, target; bool passed; };` and `Check RollCheck(const NightDef &def, NightState &state, int guest, StatKind stat, int target);` (d20 + stat >= target; no rule for 1 or 20)
  - `enum class LogKind : std::uint8_t { Opened, Check, Effect, Item, Down, Woke, /* later */ Battle, Hit, Omen, Marked, Turned, Power, Died, Ended };`
  - `struct LogEntry { std::int16_t tick; LogKind kind; std::int8_t who, other; std::int16_t a, b; };` and `NightState::log` (a ring of the last 256), `std::span<const LogEntry> NewLog(const NightState &state, std::size_t since);`
  - `void Hurt(const NightDef &def, NightState &state, int guest, int amount, int byGuest);` (`byGuest` -1 = the house): at 0 HP before the Turn the guest is `down`; `Guest::downedBy` records -1 or the guest.
  - `UseItem` on a relic: legal when a downed guest is in the same room; `b` = that guest; they wake with `rules.guests.downedReturnHp`.

Rules pinned (GDD 6, 8, 9, 11): entering an event or trap room for the first time draws the next row of the night's shuffled event or trap list and applies pass or fail effect; an item room gives the next item of the night's shuffled item list (weighted by `weight`), dropped on the floor if the guest is full; each resolves once and ends the turn. A down guest takes no turns and is skipped in the order.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("A check is d20 plus the stat against the target (T97)", "[night][check]")
// 2000 rolls for a guest with Sanity 5 against 13: passed == (die + 5 >= 13) each time;
// pass share between 60% and 70% (exact chance 65%).
TEST_CASE("An event applies its pass or its fail effect, once (T97)", "[night][check]")
// a one-row events.csv copy "sanity normal, pass item 1, fail hp -4": on a pass the guest holds
// one more item, on a fail has 4 HP less; a second guest entering later gets nothing.
TEST_CASE("A trap can bring a guest down but not kill before midnight (T97)", "[night][check]")
// a trap of fail hp -99: the guest is down, not dead, hp == 0, downedBy == -1; Current() skips them.
TEST_CASE("A relic wakes a downed guest in the same room and is used up (T97)", "[night][check]")
TEST_CASE("An item room gives one item; a full guest finds it on the floor (T97)", "[night][check]")
```

- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-107 last-curtain: checks, events, traps, down and relics`

### Task 5: Battles, buffs, levels and monsters

**Files:** Create `src/Battle.cpp`, `tests/BattleTests.cpp`; modify `Night.hpp`

**Interfaces:**
- Produces:
  - `struct Fighter { std::int8_t guest = -1, monster = -1; bool taken = false; std::int8_t side; std::array<std::int8_t, 4> buff, buffRounds; };`
  - `struct Battle { bool active = false; std::int8_t round = 1, turn = 0, count = 0; std::array<Fighter, 8> fighters; std::int8_t cell; };` in `NightState::battle`
  - `struct Monster { std::int8_t kind, cell, leavesRound; std::int16_t hp; };` and `NightState::monsters` (`std::vector<Monster>`)
  - `Action{Attack, a = monster index or -1, b = guest index or -1}` on the board starts a battle and ends the board turn. In a battle `Current()` is the fighter to act (monsters and, later, a bot Taken act inside `Apply` after a guest's action, so `Current()` always names a guest or -1); legal: `Strike{a = fighter index}`, `UseItem`, `Guard`, `Leave` (not in round 1).
  - `Action{LevelUp, a = 0 Might | 1 Sanity}` is the only legal action while a guest has a level pending.
  - `int Damage(const NightDef &def, const NightState &state, const Fighter &attacker, const Fighter &defender);`

The formulas, all from `rules.json` and the data rows (the tests pin them):

```
attack  = attacker's Might if its weapon/attack is physical, else its Sanity      (with buffs)
defense = defender's Might against physical, Sanity against magical              (with buffs)
damage  = max(battle.minDamage, battle.base + attack - defense / battle.defenseDivisor)
          halved if the defender resists that type (monster `resist`)
          divided by battle.guardDivisor if the defender is Guarded (that buff then ends)
Order in a round: the side that started the battle acts first, its fighters in board order; then the other side.
After battle.rounds rounds, or when a side has nobody standing, or after Leave: the battle ends, HP stays.
XP: each guest on the winning side gets the monster's xp when a monster is killed. No XP for guests.
Level: when xp reaches levels.xp[level-1], a level is pending: +levels.hp max HP (not healed), and the choice.
Monsters: from rules.monsters.fromRound, at the start of each round, with chancePercent, one appears in a
random opened cell without a guest, if fewer than max; hp/might/sanity grow by the row's perHour values
times hours since 19:00; it leaves at leavesRound. A guest whose Move enters its cell is in a battle at once
(the monster's side started it). A monster not killed stays with the HP it has left.
```

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("Damage follows the data formula for both types (T98)", "[night][battle]")
// khang (Might 7, physical) on a stagehand (Might m, resists physical): Damage ==
// max(1, (3 + 7 - m / 2)) / 2; thanh (Sanity 7, magical) on the same: 3 + 7 - sanity / 2.
// Changing rules.battle.base in the NightDef copy changes the result accordingly.
TEST_CASE("The starter strikes first and a battle lasts three rounds (T98)", "[night][battle]")
// guest attacks a 999-HP monster: the first LogKind::Hit is the guest's; after three rounds of
// Strike battle.active is false, the monster keeps its remaining HP, the guest's board turn is over.
TEST_CASE("Buffs show their effect and run out (T98)", "[night][battle]")
// padded coat -> Guarded: the next hit is halved, the one after is not; whetstone -> +2 Might
// until the battle ends; a monster with debuff "weakened" lowers Might by 2 for 2 rounds.
TEST_CASE("Killing monsters gives XP and levels without healing (T98)", "[night][battle]")
// xp thresholds from rules: after 2 kills LevelUp is the only legal action; choosing Might gives
// +1 Might, +4 max HP, hp unchanged.
TEST_CASE("Monsters appear, grow with the hour, leave, and force a fight when met (T98)", "[night][battle]")
// with chancePercent 100: none before fromRound, never more than max, gone after stayRounds;
// a Move into a monster's cell makes battle.active with the monster's side first.
TEST_CASE("Leaving is not possible in the first round (T98)", "[night][battle]")
```

- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement** in `Battle.cpp`.
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-108 last-curtain: battles, buffs, levels, monsters`

### Task 6: Guest against guest, bonds and allies

**Files:** Create `src/Company.cpp`, `tests/CompanyTests.cpp`; modify `Night.hpp`

**Interfaces:**
- Produces: `NightState::bonds` (`std::array<std::int16_t, maxGuests * maxGuests>`, from `bonds.csv`), `int Bond(const NightState &state, int from, int to)`, `Guest::askedUntilRound` (`std::array<std::int8_t, maxGuests>`), `Action{Persuade, a = guest}`, `bool Controls(const NightState &state, int controller, int guest)` (true for the player's own guest and members of group 0), `std::vector<int> Joiners(const NightDef &def, const NightState &state, int attacker, int defender)` (bots in the cell that may join the defender).

Rules pinned (GDD 11, 15): attacking a guest starts an ordinary battle (3 rounds, no XP). Every guest whose light reaches the cell lowers their bond to the attacker by `rules.guests.attackBondLoss`, unless the defender has already turned. A bot in the same cell may join the defender, as its own choice: before round 1, `Current()` returns each guest of `Joiners` in turn, whose only legal actions are `Join` (enter on the defender's side) and `EndTurn` (stay out); neither uses up that guest's own board turn. Persuade: same cell, ends the turn, d20 + Sanity against `persuade.target`, minus `persuade.bond` if `Bond(target, asker) > 0`, plus it if `< 0`; on success the target joins group 0 (at most 3 in the group, and only the player's guest may ask); on failure `askedUntilRound = round + persuade.retryRounds`. Allies in the starter's cell fight on the starter's side.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("Attacking a guest is slow, pays no XP and is seen (T99)", "[night][company]")
// equal guests: after one 3-round battle neither is down; xp unchanged; a watcher in light has
// Bond(watcher, attacker) lower by attackBondLoss, a guest in the dark has it unchanged.
TEST_CASE("A guest brought down by a guest stays down until midnight (T99)", "[night][company]")
// downedBy == attacker; no turns; (the wake at midnight is checked in Task 8).
TEST_CASE("Only the player persuades, at most two allies, harder against a bad bond (T99)", "[night][company]")
// player == 0: Persuade is legal for guest 0 only, never when group 0 has 3 members or before
// askedUntilRound; target = 13 - 3 with a positive bond, 13 + 3 with a negative one (read back
// from the logged Check).
TEST_CASE("Allies fight beside the player and are the player's to move (T99)", "[night][company]")
// Controls(0, ally) true; an ally in the cell appears on the starter's side of the battle.
TEST_CASE("A bot in the room may be offered the defender's side (T99)", "[night][company]")
```

- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement** in `Company.cpp`.
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-109 last-curtain: guest fights, bonds, allies`

### Task 7: Omens and the mark

**Files:** Modify `src/Company.cpp`, `Night.hpp`, `tests/CompanyTests.cpp`

**Interfaces:**
- Produces: `NightState::marked` (`std::int8_t`, -1), `omensTaken`, `Guest::sawFail` and `Guest::sawOmen` (bitmasks of guests), `Action{TakeOmen}`, `Action{LeaveOmen}` (the two legal actions on entering an unclaimed omen room; a left omen stays for the next guest), `enum class Outcome : std::uint8_t { Running, CompanyEarly, Company, Taken }`, `Outcome Result(const NightState &state)`.

Rules pinned (GDD 12): taking gives the omen's row (`might, sanity, speed, maxhp`; max HP also heals that amount) and rolls d20 + the guest's Sanity **before the gift** against `omens.targets[omensTaken]` (the last target repeats). The first failure sets `marked`. Guests whose light reaches the cell get the taker's bit in `sawOmen`, and in `sawFail` if it failed. Nothing about omens is visible through any query for a guest who did not see it (`Knows(state, viewer, guest)` returns the two bits). If a guest brings the marked guest down before the Turn, `Result` is `CompanyEarly` and `Current()` is -1.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("An omen gives its gift and costs a roll against a rising target (T100)", "[night][omen]")
// first taker rolls against 6, second against 9 ...; Sanity used is the pre-gift value even for
// the scorched playbill (+4 Sanity); the bell adds 12 max HP and 12 HP.
TEST_CASE("The first failure marks, in secret (T100)", "[night][omen]")
// force a failure (targets {99}): marked == taker; a watcher in light has sawFail bit, a guest in
// the dark has neither bit; a later failure by someone else does not move the mark.
TEST_CASE("Leaving an omen leaves it for the next guest (T100)", "[night][omen]")
TEST_CASE("Bringing the marked guest down by a guest's hand wins the night at once (T100)", "[night][omen]")
// Result == CompanyEarly, Current() == -1; the same fall to a trap leaves Result == Running.
```

- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-110 last-curtain: omens and the mark`

### Task 8: The Turn and the hunt

**Files:** Create `src/Hunt.cpp`, `tests/HuntTests.cpp`; modify `Night.hpp`

**Interfaces:**
- Produces: `struct Taken { bool present = false; std::int8_t guest, cell, cooldown = 0; std::int16_t hp, might, sanity, speed; std::int8_t movesLeft; };` in `NightState::taken`; `bool IsTaken(const NightState &state, int guest)`; after the Turn `Current()` returns the Taken's guest index on its turn (last in every round), and `Controls(player, takenGuest)` is true when the player was marked.

Rules pinned (GDD 11, 13, 14): at the end of round `clock.turnAfterRound`: (1) if `marked == -1`, mark by chance among omen holders weighted by omens held, the player included; with no omen taken, among all seven; (2) the marked guest leaves every group and becomes the Taken at the under-stage with the `taken*` columns of their row, at full HP even if they were down; (3) every other guest goes to the foyer, the downed stand with `downedReturnHp`; (4) `LogKind::Turned`. In the hunt: 0 HP is `dead` and the guest's items fall to the floor; monsters join the Taken's side in battles and attack only the company; traps and events do nothing to the Taken; a battle starts when a company guest and the Taken share a cell (whoever moved in started it) with every company guest in the cell on one side, 3 rounds, and the Taken's HP carries over. `Result`: `Company` when the Taken's HP reaches 0 with a guest alive; `Taken` when no company guest is alive or the last round ends.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("At midnight the marked guest becomes the Taken and the rest gather (T101)", "[night][hunt]")
// marked by hand: after round 10 taken.present, taken.cell is the under-stage, hp == takenHp,
// every other guest is in the foyer; a guest downed by a trap or by a guest stands with 1 HP.
TEST_CASE("With no failed roll, chance marks an omen holder; with no omen, anyone (T101)", "[night][hunt]")
// 300 seeds, two holders with 1 and 3 omens: the Taken is always one of them, the 3-omen holder
// between 65% and 85% of the time; 300 seeds with no omen: every guest is chosen at least once,
// the player's guest included.
TEST_CASE("A marked guest felled by the house returns at full strength (T101)", "[night][hunt]")
TEST_CASE("A marked ally leaves the player's group; a marked player controls the Taken (T101)", "[night][hunt]")
TEST_CASE("After midnight zero HP is death and items fall (T101)", "[night][hunt]")
TEST_CASE("Battle on contact: three rounds, the Taken's HP carries over (T101)", "[night][hunt]")
// two battles in a row lower taken.hp cumulatively; all company guests in the cell fight together.
TEST_CASE("Monsters and traps side with the Taken (T101)", "[night][hunt]")
TEST_CASE("The night ends three ways (T101)", "[night][hunt]")
// Taken at 0 with a survivor -> Company; all company dead -> Taken; round 20 over -> Taken.
```

- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement** in `Hunt.cpp`.
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-111 last-curtain: the Turn and the hunt`

### Task 9: The seven powers

**Files:** Modify `src/Hunt.cpp`, `Night.hpp`, `tests/HuntTests.cpp`

**Interfaces:**
- Produces: `Action{Power, a = target guest or cell, b = direction for seal}`; `NightState::flooded` and `sealed` (small fixed arrays of `{cell or cell+direction, untilRound}`), `Taken::reflect` (bool).

Each power is one code path chosen by `powers.json` `kind`; every number comes from that object. One use per Taken turn, at any point of it, when `cooldown == 0`; then `cooldown` = the power's `cooldown` and counts down each Taken turn (1 = every turn).

| Kind | Legal when | Effect |
|---|---|---|
| line | a company guest is `InLine` from the Taken | that guest takes `damage` at once; no battle |
| area | a company guest is within `range` steps | every company guest within `range` steps takes `damage` |
| flood | always | the Taken's cell and each cell joined to it by a passage are flooded for `rounds`: a guest there has Speed 1 and takes `damage` when their turn ends there |
| pull | a company guest is within `range` steps | that guest is moved to the Taken's cell and takes `damage`; a battle follows on contact |
| seal | a passage within `range` steps, and sealing it leaves every living company guest a path to the Taken | that passage is impassable for `rounds` |
| swap | a company guest stands in an opened cell | the Taken and that guest change places; the Taken may still move and act |
| reflect | always | the next strike or power damage on the Taken this round is dealt to its user instead |

- [ ] **Step 1: Write the failing tests** — one `TEST_CASE("Power: <kind> (T102)", "[night][power]")` per row, each on a hand-built board, asserting the "legal when" both ways and the effect with the numbers read from `def.powers`, plus:

```cpp
TEST_CASE("A power is offered only with a valid target and respects its cooldown (T102)", "[night][power]")
// line with nobody in line: no Power in LegalActions; cooldown 2 in a copy: legal every second turn.
TEST_CASE("A seal can never cut the whole company off from the Taken (T102)", "[night][power]")
// a board where one passage is the only way: sealing it is not legal.
TEST_CASE("Changing a power's numbers in the data changes the game, not the code (T102)", "[night][power]")
// area damage 5 -> 9 in a NightDef copy: the same action now removes 9 HP.
```

- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-112 last-curtain: the seven powers`

### Task 10: Fingerprint and saves

**Files:** Create `src/NightSave.cpp`, `tests/SaveTests.cpp`; modify `Night.hpp`

**Interfaces:**
- Produces: `std::uint64_t Fingerprint(const NightState &state);` `nlohmann::json SaveNight(const NightDef &def, const NightState &state);` `bool LoadNightState(const NightDef &def, const nlohmann::json &saved, NightState &out, std::vector<core::Diagnostic> &diagnostics);` (refuses a save made with other data: `NightDef::fingerprint`, a hash of every loaded value, is stored and compared; code `night.save`).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("Same data and seed, same night (T103)", "[night][save]")
// two states driven by the same fixed chooser (always LegalActions()[Dice % size]) end with equal
// Fingerprint; a golden value for seed 1 is recorded here and updated whenever data or rules change.
TEST_CASE("A save at any point continues to the same end (T103)", "[night][save]")
// for seed 1..20: drive to a random Apply count (covering mid-battle, the Turn, the hunt, a
// pending level), SaveNight -> dump -> parse -> LoadNightState, drive both on: equal Fingerprint.
TEST_CASE("A damaged save or one from other data is refused (T103)", "[night][save]")
```

- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-113 last-curtain: fingerprint and saves`

### Task 11: The bots

**Files:** Create `modules/nightbots/CMakeLists.txt`, `include/NightBots.hpp`, `src/NightBots.cpp`, `tests/BotTests.cpp`

**Interfaces:**
- Consumes: `LegalActions`, `Apply`, `Current`, `Knows`, `Lit`, `Steps`, `Stat`, `Result`.
- Produces: `Action ChooseAction(const NightDef &def, const NightState &state, int guest);` `void PlayNight(const NightDef &def, NightState &state);` (every guest by `ChooseAction` until `Result != Running`; safe against a stuck night: at most 5,000 Applies, then it stops and the soak counts it as a fault).

How a bot chooses (the part the tests do not determine):

```
1. A bot knows only what is in its light and what it saw (Knows). It never reads `marked`.
2. Score every legal action with its style row (styles.csv):
     opening a doorway: explore;  entering an item room / picking up: loot;
     a monster it can beat (its damage x3 >= monster hp, or hp above half): fight;
     taking an omen: omen (leaving it otherwise);  ending in an unused bonus room: bonus;
     moving toward a guest it dislikes: -avoidGuests.
3. Hard rules before scores:
     hp below fleeBelowHpPercent -> use a heal item, else move away from monsters and attackers;
     it saw a guest fail an omen roll -> move toward and attack that guest (before the Turn);
     it is marked and someone saw it fail -> move to the darkest reachable cell from all others;
     attacked by a guest -> temper `fight`: strike back; `flee`: Leave when legal, then move away;
     offered the defender's side -> join if Bond(self, defender) > Bond(self, attacker).
4. In a battle: strike the fighter with the least HP; use a boost in round 1 if held; heal below 40%.
5. In the hunt (company): move toward the Taken when hp is above half and another guest is within
   2 steps, else toward the nearest guest; as the Taken: use the power when legal (on the guest
   with the least HP), else approach a lone guest, else keep away from three or more together.
6. Ties: the first in LegalActions order (no dice, so a bot is deterministic).
```

The numbers in steps 3-5 (40%, half, 2 steps, three or more) are read from `styles.csv` columns added for them; they are written here as their first values.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("Seven bots finish a night on every seed (T104)", "[night][bots]")
// seed 1..300: PlayNight ends with Result != Running in under 5000 Applies; round <= 20.
TEST_CASE("A bot uses only what it could have seen (T104)", "[night][bots]")
// two states equal except `marked` (set by hand, nobody saw): every bot chooses the same action.
TEST_CASE("A bot that saw the failure hunts; a seen marked bot hides (T104)", "[night][bots]")
TEST_CASE("Bots do not attack guests without cause (T104)", "[night][bots]")
// 200 nights where no failure is ever seen (light.steps 0 in the copy): no guest-on-guest Attack in any log.
TEST_CASE("Styles differ: the explorer opens more rooms than the fighter (T104)", "[night][bots]")
TEST_CASE("A hurt bot heals or flees (T104)", "[night][bots]")
```

- [ ] **Step 2: Run** — Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** — Expected: PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-114 last-curtain: bots`

### Task 12: The soak test, the tools, and the first balance pass

**Files:** Create `include/NightSoak.hpp`, `src/NightSoak.cpp`, `tool/NightCommand.cpp`, `include/Night.md`, `include/Balance.md`; modify `tests/BotTests.cpp`, `docs/ROADMAP.md`, `docs/current-engine-status.md`, `docs/CONTEXT.md`, `CLAUDE.md`; create `docs/testing/s25-night-simulation.md`

**Interfaces:**
- Produces:
  - `struct SoakForm { int nights, companyWins, earlyWins; };`
  - `struct SoakReport { int nights, faults; std::array<SoakForm, maxGuests> forms; /* by marked guest */ int earlyWins, companyWins, takenWins; long long rooms, omens, monsterKills, levels, downs, deaths, guestAttacks; std::array<int, maxGuests> marked; bool Holds(const NightDef &def) const; };`
  - `SoakReport Soak(const NightDef &def, std::uint64_t firstSeed, int nights, int strategy = 0);` (`strategy` swaps guest 0's chooser: 0 = its bot, 1 = attacks the nearest guest every turn, 2 = refuses every omen, 3 = takes every omen)
  - `std::string Text(const NightDef &def, const SoakReport &report);` `nlohmann::json Json(...)`
  - Tools: `night-check <data folder>` (load and validate; exit 1 with diagnostics); `night-run [--data <folder>] [--seed 1] [--player <guest id>] [--json]` (one bot night as a readable log: clock, who did what, checks with die and target, the Turn, the end); `night-soak [--data <folder>] [--nights 1000] [--seed 1] [--strategy 0] [--json]` (exit 1 when `Holds` is false).

`Holds`: no faults, and for every form with at least 50 hunts the company's share of hunt wins is within `rules.balance.companyWinMin..Max`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("The soak report adds up (T105)", "[night][soak]")
// 200 nights: nights == earlyWins + companyWins + takenWins + faults; sum of forms[].nights ==
// nights - earlyWins - faults; sum of marked[] == nights - faults.
TEST_CASE("A thousand nights have no faults (T105)", "[night][soak]")
// faults == 0; every guest is marked at least 20 times; rooms/nights >= 12.
TEST_CASE("The shipped data is balanced (T105)", "[night][soak][balance]")
// Soak(def, 1, 1000).Holds(def); and against strategies over 500 nights each:
// guest 0 wins less often attacking everyone (1) than playing its bot (0);
// refusing every omen (2) is not better than the bot by more than 5 points;
// taking every omen (3) does not put the company above companyWinMax.
```

- [ ] **Step 2: Run** — Expected: the first two pass once implemented; the balance case is expected to FAIL on the first data.
- [ ] **Step 3: Implement** `NightSoak.cpp` and `NightCommand.cpp`.
- [ ] **Step 4: Balance pass.** Change **only files under `assets/night/`** until the balance case passes. Work in this order and record each change and its effect in `docs/testing/s25-night-simulation.md`: (1) `guests.csv` taken columns so the overall hunt is near 50%; (2) `powers.json` per form until every form is inside the band; (3) `omens.csv` and `rules.json` `omens.targets` for the omen strategies; (4) `monsters.csv` and `levels` so a fighting guest reaches level 5 in about 15 fighting turns. If a number cannot be reached through data, stop and write down which mechanic blocks it; do not hard-code.
- [ ] **Step 5: Write `Night.md`** (the rules as built, with the file that owns each number) **and `Balance.md`** (for each line of the soak report: which file and column moves it; the tuning order above; how to run a variant folder with `--data`).
- [ ] **Step 6: Run everything:** `bash /home/claude/verify.sh > /tmp/verify.log; tail -4 /tmp/verify.log` — Expected: `BUILD OK` and all tests passed for gcc, clang and asan. Then `UmbraTool night-soak` — Expected: ends with "the data holds", exit 0.
- [ ] **Step 7: Update the records** (ROADMAP status row and current next task, engine status, CONTEXT, CLAUDE.md first-use line, the testing record with the soak report and a Windows checklist: build, `Umbra.Night`, `night-soak` giving the same numbers as the cloud).
- [ ] **Step 8: Commit** `issue #UMBRA-115 last-curtain: soak test, tools, first balance pass`

---

## What plan 1 leaves for the later plans

- The player: plan 1's player is a bot or a scripted strategy. Plan 2 puts a person behind the same `LegalActions`/`Apply`, with allies via `Controls`.
- What the player is shown: plan 2 draws only what `Lit` and `Knows` allow, and speaks a bot's `Action` when it is in light.
- Lines and tales: plan 3 turns `LogEntry` rows and bot `Action`s into words through the existing model and guard.
- The under-stage and the opera house's look, the guests and the Taken forms: plan 4.
