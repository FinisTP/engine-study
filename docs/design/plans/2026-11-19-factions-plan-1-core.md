# Faction matches, Plan 1: the 3v3 core slice — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A headless, data-driven 3v3 faction match (mirrored lane board, essence, the moving stage,
three-round battles, board abilities, basic items and events, monsters with behaviours, guardians,
bots at three levels) that the soak can play to the end on any seed.

**Architecture:** Two new modules beside the old night (which stays untouched until the app moves
over in Plan 2): `modules/match` (namespace `umbra::match`, library `Umbra::Match`) holds the data,
the board generator and the rules as one copyable `MatchState` changed only by `Apply`;
`modules/matchbots` (`Umbra::MatchBots`) holds the bots and the soak. Data lives in
`assets/match/`. Tests are a new `UmbraMatchTests` executable.

**Tech Stack:** C++20, CMake, nlohmann_json, Catch2 3, the night module's CSV reader.

**Spec:** `docs/design/2026-11-19-factions.md`

**Later plans (not here):** Plan 2, the app (board view with tinted Huy and specter sprites, the
pick screen, the forecast panel, the battle screen, stage glow, countdown), saves, the 1v1
tutorial. Plan 3, special effects (skip, push last, the follow-up passive, more item classes),
retiring the old night module.

## Global Constraints

- C++20; one static library per module; headers under `include/`, included by bare name.
- Short, simple code; non-obvious code gets `// 1. ...` step comments; math and deeper ideas go in a
  markdown file in the same directory (the board generator's mirror and distance check: `Board.md`).
- C++ string and char literals ASCII only.
- The rules build and run without Vulkan, a window, a model, Python or a network.
- Every number in the spec is data: `assets/match/options.csv`, `rules.json` and the CSVs. No
  gameplay number in C++.
- Everything follows from the seed: same data, seed and actions, same match.
- Player input and bots submit the same `Action`s; only `Apply` changes the state.
- Commits: `issue #UMBRA-<n> <summary>`, ending with the Co-Authored-By and Claude-Session lines.
  Issues #251 to #260, one per task. Tests numbered from T274.
- Five stats: HP, Attack, Defence, Speed, Move. No Sanity, Might or MP.
- Damage = power (or a dice roll) + Attack − Defence (+ shield), at least `battle.minDamage`.

## Review Focus

1. A fighter knocked out on its own turn (the attacker loses its battle): the turn must pass to the
   next character, and its pending level choice must not block the match. → T290 (Task 5).
2. The stage moves while someone stands on it, or the next stage room holds a monster: the old
   occupant stays where it is, and the new stage only blocks entry. → T298 (Task 7).
3. A board with too few stage rooms: the generator must give at least two equal-distance candidates
   or reject the seed and try the next. → T279 (Task 2).
4. A data set where a role has no free (0 HP) battle ability: the loader refuses it, so a fighter at
   1 HP always has a move. → T276 (Task 1).
5. A bot whose every ability is unaffordable: `EndTurn` (or a free ability in battle) is always
   legal, so no match stalls. → T305 (Task 9).

---

### Task 1: The data and its loader (#251)

**Files:**
- Move: `modules/night/src/Csv.hpp` → `modules/night/include/Csv.hpp` (add it to the night FILE_SET; no other change)
- Create: `modules/match/CMakeLists.txt`, `modules/match/include/MatchDef.hpp`, `modules/match/src/MatchData.cpp`, `modules/match/include/Match.md`
- Create: `assets/match/options.csv`, `rules.json`, `characters.csv`, `jobs.csv`, `abilities.csv`, `monsters.csv`, `items.csv`, `events.csv`
- Modify: `plugin.json` (modules `match`, `matchbots`; links `Umbra::Match`, `Umbra::MatchBots`), `tests/CMakeLists.txt` (new `UmbraMatchTests`, `add_test(NAME Umbra.Match ...)`, and `Umbra.Match.Balance` for `[match-balance]`)
- Test: `tests/MatchDataTests.cpp`, `tests/MatchTestData.hpp` (`Data()` loads `UMBRA_MATCH_DATA`)

**Interfaces:**
- Produces, in `MatchDef.hpp`:
  - `enum class Role { Attacker, Defender, Supporter, Speedster, AllRounder };`
  - `enum class Use { Battle, Board, Passive };`
  - `enum class AbilityClass { Strike, Shield, Heal, Hit, FirstStrike };` (Strike, Shield: battle; Heal: battle or board; Hit: board; FirstStrike: passive)
  - `struct Dice { int count = 0, sides = 0; };`
  - `struct AbilityDef { std::string id, name, animation; Use use; AbilityClass kind; int hpCost = 0, power = 0, range = 0; Dice dice; };` (dice used when `count > 0`, instead of `power`)
  - `struct CharacterDef { std::string id, name, sprite; Role role; int hp, attack, defence, speed, move; std::uint32_t tint; };`
  - `struct JobLevel { Role role; int level, ability, choice = -1; };` (level 1 rows are the starting abilities)
  - `enum class Tier { Weak, Strong, Great, Guardian };` `enum class Behaviour { Stay, Flee, Stage, Guard };`
  - `struct MonsterDef { std::string id, name, sprite; Tier tier; Behaviour behaviour; int hp, attack, defence, speed, essence, xp, respawnRounds; std::vector<int> abilities; std::uint32_t tint; };`
  - `enum class ItemClass { HealItem, ExpGrant };` `struct ItemDef { std::string id, name, text, sprite; ItemClass kind; int amount; };`
  - `enum class Effect { None, Hp, Essence, Xp, Item };` `struct Outcome { Effect effect = Effect::None; int amount = 0; };` (Item: `amount` is the items.csv row)
  - `struct Choice { std::string label; int chance; Outcome success, failure; };` `struct EventDef { std::string id, name, text; std::vector<Choice> choices; };`
  - `enum class BotLevel { Easy, Medium, Hard };`
  - `struct Options { int rounds = 30, side = 3; bool autoEndTurn = false; BotLevel bots = BotLevel::Medium; };`
  - `struct Rules` with nested structs: `board { width, height, lanes, weakMin, weakMax, strongMin, strongMax, weakPerHalf, strongPerHalf, eventsPerHalf, eventGap, itemsPerHalf }`, `stage { openRounds }`, `respawn { turns }`, `battle { rounds, minDamage }`, `levels { std::vector<int> xp; hp; killXp }`, `great { wakeRound }`, `carry`, `bots { std::array<int, 3> noise }`, `balance { sideMin, sideMax, firstMoverMax, sourceMax, comebackMin }`
  - `struct MatchDef { Options options; Rules rules; std::vector<CharacterDef> characters; std::vector<JobLevel> jobs; std::vector<AbilityDef> abilities; std::vector<MonsterDef> monsters; std::vector<ItemDef> items; std::vector<EventDef> events; template<class T> static int Find(const std::vector<T>&, std::string_view id); };`
  - `bool LoadMatch(const std::filesystem::path &folder, MatchDef &out, std::vector<core::Diagnostic> &diagnostics);` (diagnostic code `match.<file>`, field `<file>:<row>:<column>`, as the night loader)

**Data (the first, basic set):**
- `options.csv` (`id,value`): `rounds,30` `side,3` `autoEndTurn,0` `bots,medium`.
- `rules.json`: board 9 wide, 15 high, 3 lanes, weak 2-4, strong 5-7, 3 weak and 1 strong per half, 2 events per half at least 2 apart, 2 items per half; stage openRounds 6; respawn turns 1; battle rounds 3, minDamage 1; levels xp `[3, 7, 12, 18]`, hp 2 per level, killXp 3; great wakeRound 20; carry 4; bots noise `[60, 25, 0]`; balance 45/55/5/60/25.
- `characters.csv`: five, one per role, reusing cast names: `bao` Attacker, `khang` Defender, `mai` Supporter, `lan` Speedster, `huy` All-Rounder; `sprite` = their own id (drawn overnight; `huy` with a tint until then).
- `abilities.csv`: per role a free Strike (hpCost 0), one costed battle ability, one board ability (Hit for Attacker, Speedster and All-Rounder; Heal for Supporter; Shield is battle-only for Defender); `Duel` FirstStrike passive for the Speedster at level 3; one dice Strike (`2d6`).
- `jobs.csv`: per role, level 1 the free Strike and the board ability, level 2 a choice of two battle abilities, level 4 one more.
- `monsters.csv`: `moths` Weak Stay, `stagehand` Strong Stay, `spider` Great Flee, `usher` Guardian Guard, `prompter` Strong Stage; all `sprite` = `specter`, distinct tints.
- `items.csv`: `bandage` HealItem 6, `programme` ExpGrant 3.
- `events.csv` (`id,name,text,label1,chance1,success1,failure1,label2,chance2,success2,failure2`; outcomes `hp:-3`, `essence:2`, `xp:2`, `item:bandage`, `none`): two events from the old set, rewritten as choices.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("The shipped match data loads (T274)", "[match][data]") {
    const MatchDef def = Data();
    CHECK(def.options.rounds == 30); CHECK(def.options.side == 3);
    CHECK_FALSE(def.options.autoEndTurn); CHECK(def.options.bots == BotLevel::Medium);
    CHECK(def.characters.size() == 5);
    for (Role r : {Role::Attacker, Role::Defender, Role::Supporter, Role::Speedster, Role::AllRounder})
        CHECK(std::ranges::count(def.characters, r, &CharacterDef::role) == 1);
    const AbilityDef &d = def.abilities[MatchDef::Find(def.abilities, "gamble")];
    CHECK(d.dice.count == 2); CHECK(d.dice.sides == 6);
}
TEST_CASE("Bad match data names its file and field (T275)", "[match][data]") {
    // copies the folder to a temp dir, breaks one cell each time, and expects LoadMatch false with
    // the diagnostic's field: an unknown ability in jobs.csv ("jobs.csv:2:3"), a chance of 120 in
    // events.csv, dice "2x6", options rounds 0, an unknown behaviour in monsters.csv.
}
TEST_CASE("Every role starts with a free battle ability (T276)", "[match][data]") {
    // removes the Defender's free Strike row from jobs.csv: LoadMatch fails with code "match.jobs".
}
```

- [ ] **Step 2: Run them to see them fail** — `cmake --build /home/claude/build-gcc --target UmbraMatchTests` → Expected: build fails, `MatchDef.hpp` not found.
- [ ] **Step 3: Implement `LoadMatch`** in `MatchData.cpp` with `umbra::night::ReadCsv`, the night loader's `Fail`/`ParseInt` pattern, and the checks above (ids resolve, chances 0-100, dice `NdM`, rounds 1-60, side 1-5, each role has a level-1 battle ability of hpCost 0). Write the data files. Start `Match.md` with a data table (which file owns which number).
- [ ] **Step 4: Run** `/home/claude/build-gcc/plugins/last-curtain/tests/UmbraMatchTests "[data]"` → Expected: 3 test cases pass. Run the night suite too → Expected: unchanged (the Csv move breaks nothing).
- [ ] **Step 5: Commit** `issue #UMBRA-251 The faction match's data and loader (T274-T276)`.

### Task 2: The mirrored lane board (#252)

**Files:**
- Create: `modules/match/include/Match.hpp` (board part), `modules/match/src/Board.cpp`, `modules/match/include/Board.md`
- Test: `tests/MatchBoardTests.cpp`

**Interfaces:**
- Consumes: `MatchDef`, `Rules::board`.
- Produces:
  - `enum class CellKind : std::uint8_t { Wall, Hall, Room, Centre, Hub };`
  - `struct Cell { CellKind kind = CellKind::Wall; std::uint8_t doors = 0; std::int8_t event = -1; bool resolved = false; };` (doors bit 0 = N, then E, S, W)
  - `struct Board { int width = 0, height = 0; std::vector<Cell> cells; std::array<std::int16_t, 2> hub{}; std::vector<std::int16_t> stageRooms, weakDens, strongDens; };` (side 0's hub at the bottom row, side 1's at the top; dens listed for both halves)
  - `Board BuildBoard(const MatchDef &def, std::uint64_t seed);`
  - `int Mirror(const Board &b, int cell);` (`x, y` → `x, height - 1 - y`, like a chessboard)
  - `int Steps(const Board &b, int from, int to);` (breadth-first through doors; -1 if unreachable)
  - `bool Fair(const Board &b);` (every cell's steps to hub 0 equal its mirror's steps to hub 1; same kinds, doors mirrored, events and dens mirrored)

**Generator (the one algorithm the tests do not fix):** 1. lay hub 0 in the bottom row's middle and
`lanes` hallway lanes up from it to the middle row (left, middle, right); 2. turn the middle rows
into `Centre` cells joined on all four sides; 3. hang `Room` cells off the lanes; 4. place weak and
strong dens, events and items in rooms by their step bands from hub 0, events at least `eventGap`
apart; 5. mirror the bottom half onto the top; 6. stage rooms are the Centre cells with four doors
and equal steps to both hubs; 7. if `Fair` fails or fewer than two stage rooms exist, retry with the
next seed (at most 64 tries, then the last board is returned and `Fair` is false). The mirror and the
distance proof go in `Board.md`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("Boards are mirrored and fair on every seed (T277)", "[match][board]") {
    for (std::uint64_t seed = 1; seed <= 200; ++seed) {
        const Board b = BuildBoard(Data(), seed);
        REQUIRE(Fair(b));
        CHECK(b.cells[b.hub[1]].kind == CellKind::Hub);
        CHECK(Mirror(b, b.hub[0]) == b.hub[1]);
        CHECK(Steps(b, b.hub[0], b.hub[1]) > 0);
    }
}
TEST_CASE("Dens sit in their step bands from their own hub (T278)", "[match][board]") {
    // for each den in the bottom half: weakMin <= Steps(hub0, den) <= weakMax (strong likewise);
    // the den count per half equals weakPerHalf / strongPerHalf; events are eventGap apart.
}
TEST_CASE("Every board has at least two open stage rooms (T279)", "[match][board]") {
    // for seeds 1-200: stageRooms.size() >= 2; each is Centre, doors == 0xF, equal steps to both hubs.
}
```

- [ ] **Step 2: Run** `UmbraMatchTests "[board]"` → Expected: FAIL (no `BuildBoard`).
- [ ] **Step 3: Implement** `BuildBoard`, `Mirror`, `Steps`, `Fair` in `Board.cpp`; write `Board.md`.
- [ ] **Step 4: Run** `UmbraMatchTests "[board]"` → Expected: pass.
- [ ] **Step 5: Commit** `issue #UMBRA-252 The mirrored lane board (T277-T279)`.

### Task 3: A new match: sides, the rigged pick, turn order, placement (#253)

**Files:**
- Modify: `modules/match/include/Match.hpp`; Create: `modules/match/src/Match.cpp`
- Test: `tests/MatchSetupTests.cpp`

**Interfaces:**
- Consumes: `BuildBoard`, `MatchDef`.
- Produces:
  - `struct Unit { std::int8_t kind = 0 /*0 character, 1 monster*/, def = -1, faction = -1 /*-1 neutral*/; std::int16_t cell = -1, home = -1, hp = 0, maxHp = 0, attack = 0, defence = 0, speed = 0, move = 0, xp = 0; std::int8_t level = 1, back = 0 /*turns until it returns*/, pending = -1 /*jobs row awaiting a choice*/, respawnRound = 0; int essence = 0, kills = 0; std::array<std::int8_t, 8> abilities; std::array<std::int8_t, 4> items; };` (abilities and items filled with -1)
  - `struct Floor { std::int16_t cell = -1; std::int8_t item = -1; int essence = 0; };`
  - `struct MatchState { Board board; std::vector<Unit> units; std::vector<std::int8_t> order; int turn = 0, round = 1; std::array<int, 2> score{}; std::int16_t stage = -1, nextStage = -1; int stageCloses = 0; std::vector<Floor> floor; Battle battle; TurnState now; std::vector<LogEntry> log; std::uint64_t rng = 0; bool over = false; int winner = -1; };` (`Battle`, `TurnState`, `LogEntry` are declared here and filled in by Tasks 4 and 5)
  - `MatchState NewMatch(const MatchDef &def, std::uint64_t seed, std::span<const int> picks);` (`picks`: side 0's characters.csv rows, `options.side` of them, no repeats; side 1 is drawn by `RiggedPick` and may share characters with side 0)
  - `std::vector<int> RiggedPick(const MatchDef &def, std::uint64_t seed, int count);` (no role twice; Defender, Supporter, All-Rounder and Attacker first, then the rest)
  - `int TurnsLeft(const MatchDef &def, const MatchState &state);` (`options.rounds - round + 1`)

**Rules:** characters start on their hub and the squares next to it (one per square); each side's guardian on the hub's door cell (the cell
between the hub and its lanes), faction = that side; weak and strong monsters on their dens,
neutral; items on the floor of their rooms; `order` = character units by Speed, highest first, ties
shuffled from the seed; the stage opens in a random stage room with `stageCloses = 1 + openRounds`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("A match starts with both sides at home (T280)", "[match][setup]") {
    const MatchDef def = Data();
    const MatchState s = NewMatch(def, 7, std::vector<int>{0, 1, 2});
    // 3 characters per side, each on its own hub, full HP; one guardian per side next to its hub;
    // monsters on every den; s.stage is one of s.board.stageRooms; TurnsLeft(def, s) == 30.
}
TEST_CASE("The bot side never repeats a role (T281)", "[match][setup]") {
    for (std::uint64_t seed = 1; seed <= 100; ++seed) {
        const auto pick = RiggedPick(Data(), seed, 3);
        // 3 distinct roles, none of them Speedster (the four come first for 3v3)
    }
}
TEST_CASE("Turn order follows Speed, ties by the seed (T282)", "[match][setup]") {
    // order is sorted by speed, descending; two characters of equal speed are ordered differently
    // on some seed in 1-50, and the same way on a repeated seed.
}
```

- [ ] **Step 2: Run** `UmbraMatchTests "[setup]"` → Expected: FAIL.
- [ ] **Step 3: Implement** `NewMatch`, `RiggedPick`, `TurnsLeft` in `Match.cpp` (a small `Rng` from the seed, as the night's `Randomness` section).
- [ ] **Step 4: Run** → Expected: pass.
- [ ] **Step 5: Commit** `issue #UMBRA-253 A new match: sides, the rigged pick, the turn order (T280-T282)`.

### Task 4: The turn: move, undo, act, end turn, the end of the match (#254)

**Files:**
- Modify: `Match.hpp`; Create: `modules/match/src/Turn.cpp`
- Test: `tests/MatchTurnTests.cpp`

**Interfaces:**
- Produces:
  - `struct TurnState { std::int16_t origin = -1; std::int8_t movesLeft = 0; bool acted = false; };`
  - `enum class ActionKind : std::uint8_t { Move, Undo, Attack, BattleAbility, BoardAbility, UseItem, PickUp, Perform, Answer, LevelUp, EndTurn };`
  - `struct Action { ActionKind kind = ActionKind::EndTurn; std::int8_t a = -1, b = -1; bool operator==(const Action &) const = default; };` (Move a = direction; Attack a = unit; BattleAbility a = ability slot; BoardAbility a = slot, b = target unit; UseItem a = slot; PickUp a = floor index; Answer a = choice; LevelUp a = 0 or 1)
  - `enum class LogKind : std::uint8_t { Moved, Battle, Blow, Down, Essence, Performed, Level, Back, StageOpen, StageNext, Event, Item, Over };` `struct LogEntry { LogKind kind; std::int16_t a = -1, b = -1, c = 0; };`
  - `int Current(const MatchState &s);` (a unit with a pending level choice first; then the battle's fighter to pick; then `order[turn]`; -1 when over)
  - `std::vector<Action> Legal(const MatchDef &def, const MatchState &s);`
  - `bool Apply(const MatchDef &def, MatchState &s, const Action &a);` (false and no change when not legal)
  - `enum class Why : std::uint8_t { Ok, NotYourTurn, NoMovesLeft, Blocked, AlreadyActed, OutOfRange, OwnSide, TooCostly, NotOnStage, NothingToPerform, NothingHere, BagFull, MustChoose };`
    `Why Check(const MatchDef &def, const MatchState &s, const Action &a);` (`Legal` and `Apply` use it; the app's tooltip shows its reason: the user's design rule)

**Rules (spec, The match):** one unit per square (`rules.json` `board.squareCapacity` 1); a step costs 1 of `move`; the stage cell cannot be entered while
another unit stands on it, nor an enemy hub while its guardian stands; `Undo` returns to `origin` with the full
budget, only before acting; after an action no `Move`; `EndTurn` is always legal for the current
character; with `options.autoEndTurn` the turn ends by itself after the action. When the last in
`order` ends, monsters act (Task 8, a no-op until then) and the round advances; after round
`options.rounds` the match is over: most score wins, a tie goes to the side with its guardian
standing, then more kills, else `winner = -1` (draw).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("A move can be undone until the turn's action (T283)", "[match][turn]") {
    // Move twice; Undo -> back on origin, movesLeft == move; Move, Perform-less action (PickUp of an
    // item placed under the unit) -> Legal has no Move and no Undo; Apply(Move) returns false.
}
TEST_CASE("A character moves at most its Move a turn (T284)", "[match][turn]") {
    // after `move` steps, every Move is illegal; EndTurn passes Current to order[1].
}
TEST_CASE("Auto end turn ends the turn after the action (T285)", "[match][turn]") {
    // with def.options.autoEndTurn = true, the PickUp action moves Current to the next character.
}
TEST_CASE("The match ends after its rounds, with the tie rule (T286)", "[match][turn]") {
    // def.options.rounds = 2; play EndTurn only: over after 2 rounds, TurnsLeft counts 2 -> 1;
    // score equal, side 1's guardian hp set to 0 beforehand -> winner == 0; both standing,
    // equal kills -> winner == -1.
}
```

- [ ] **Step 2: Run** `UmbraMatchTests "[turn]"` → Expected: FAIL.
- [ ] **Step 3: Implement** `Current`, `Legal`, `Apply` for Move, Undo, PickUp, EndTurn, the round advance and the end, in `Turn.cpp`. Other kinds return false until their tasks.
- [ ] **Step 4: Run** → Expected: pass.
- [ ] **Step 5: Commit** `issue #UMBRA-254 The turn: move, undo, act, end turn, the end (T283-T286)`.

### Task 5: Battles, the forecast, knock-outs and respawn (#255)

**Files:**
- Modify: `Match.hpp`; Create: `modules/match/src/Battle.cpp`
- Test: `tests/MatchBattleTests.cpp`

**Interfaces:**
- Produces:
  - `struct Battle { bool on = false; std::int8_t attacker = -1, defender = -1, round = 0, next = 0 /*0 attacker, 1 defender*/, first = 0; std::array<std::int16_t, 2> shield{}; };`
  - `struct Forecast { int dealtMin = 0, dealtMax = 0, healed = 0, cost = 0; };`
  - `Forecast Preview(const MatchDef &def, const MatchState &s, const Action &a);` (BattleAbility, BoardAbility, UseItem; zeros for others)
  - `int Damage(const MatchDef &def, const MatchState &s, int ability, int from, int to, int roll);` (roll used for dice abilities)

**Rules (spec, Characters, stats and fights):** `Attack` targets a unit exactly 1 step away that is not
of the attacker's faction; it starts a battle and is the turn's action. `battle.rounds` rounds;
in each, the attacker picks first, unless the defender has a FirstStrike passive and the attacker
does not. Strike deals `Damage`; Shield adds its power to that side's shield for the battle; Heal
adds power HP up to max. An ability costs its hpCost and is only legal if `hp > hpCost`. Monsters
pick their first affordable Strike. HP 0 ends the battle: the loser is down. A character downed by a
character gives it all its essence; by a monster, drops it on the floor. A downed monster gives its
essence and XP to the character; weak monsters set `respawnRound`. A downed character leaves the
board (`cell = -1`, `back = respawn.turns`); on its next turn in order, `back` counts down and the
turn passes; at 0 it stands on its hub with full HP. Stepping onto your own hub restores full HP.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("Damage is power plus Attack minus Defence, at least 1 (T287)", "[match][battle]") {
    // Strike power 4, attack 5, defence 3 -> 6; defence 20 -> 1; a shield of 2 -> 4;
    // the dice Strike's Preview gives dealtMin 2 + 5 - 3 = 4, dealtMax 12 + 5 - 3 = 14.
}
TEST_CASE("A battle runs three rounds, attacker first, one pick each turn (T288)", "[match][battle]") {
    // Current alternates attacker, defender six times; the log has six Blow entries; then the
    // battle is off and the attacker's turn has acted. A FirstStrike defender picks first.
}
TEST_CASE("A carrier downed by a rival gives up its essence; by a monster, drops it (T289)", "[match][battle]") {
    // victim essence 9 -> killer essence 9, victim 0; downed by a monster -> a Floor of 9 on its cell,
    // which PickUp takes.
}
TEST_CASE("A downed fighter misses one turn and returns home whole (T290)", "[match][battle]") {
    // the attacker loses its own battle with a level choice pending: Current moves on to the next
    // character; at its next turn it is skipped (log Back); the turn after, it stands on its hub at
    // max HP; its pending choice is still asked first.
}
TEST_CASE("Stepping onto your own hub restores full HP (T291)", "[match][battle]") {}
```

- [ ] **Step 2: Run** `UmbraMatchTests "[battle]"` → Expected: FAIL.
- [ ] **Step 3: Implement** in `Battle.cpp`; add Attack and BattleAbility to `Legal`/`Apply`.
- [ ] **Step 4: Run** `UmbraMatchTests` (all) → Expected: pass.
- [ ] **Step 5: Commit** `issue #UMBRA-255 Battles, the forecast, knock-outs and respawn (T287-T291)`.

### Task 6: Levels, board abilities, items and events (#256)

**Files:**
- Create: `modules/match/src/Abilities.cpp`
- Test: `tests/MatchAbilityTests.cpp`

**Rules (spec):** XP from monsters (their xp) and character kills (`levels.killXp`); at each
`levels.xp` step the level rises, max HP and HP rise by `levels.hp`, and the job's row for that level
is learnt; a row with a `choice` sets `pending`, answered at once by `LevelUp` (0 the ability, 1 the
choice). Board abilities are the action: Hit deals `Damage` to an enemy unit within `range` (it can
down it, with Task 5's rules); Heal gives `power` HP to an ally within `range` (the user may target
itself), costing the user its hpCost. `UseItem`: HealItem heals `amount`, ExpGrant gives `amount`
XP; carry at most `rules.carry` items. `Answer` on an unresolved event cell: roll `chance` (0-100)
from the match's random numbers, apply success or failure, mark the cell resolved.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("Levelling learns the job's ability, or asks for a choice (T292)", "[match][levels]") {
    // xp to levels.xp[0]: level 2, maxHp + 2; the level-2 row has a choice: Current is that unit,
    // Legal is exactly {LevelUp 0, LevelUp 1}; LevelUp 1 puts the choice ability in a free slot.
}
TEST_CASE("Board abilities: Hit at range, Heal an ally at the healer's cost (T293)", "[match][abilities]") {
    // Hit out of range: Check == Why::OutOfRange; cost above HP: Why::TooCostly; in range it deals Damage; Heal on an ally +power, healer -hpCost.
}
TEST_CASE("Items heal and grant XP (T294)", "[match][items]") {}
TEST_CASE("An event's choice rolls its chance and resolves the room (T295)", "[match][events]") {
    // chance 100 -> success applied; chance 0 -> failure; the cell is resolved and Answer is gone.
}
```

- [ ] **Step 2: Run** → Expected: FAIL. **Step 3: Implement.** **Step 4: Run** all → Expected: pass.
- [ ] **Step 5: Commit** `issue #UMBRA-256 Levels, board abilities, items and events (T292-T295)`.

### Task 7: The stage and Perform (#257)

**Files:**
- Create: `modules/match/src/Stage.cpp`
- Test: `tests/MatchStageTests.cpp`

**Rules (spec, The stage):** one unit at a time stands on the stage; standing scores nothing.
`Perform` (on the stage, essence > 0) adds the unit's essence to its faction's score and empties it;
it is the turn's action. At the start of round `stageCloses - 1` the next room is picked from the
other stage rooms and logged (`StageNext`); at round `stageCloses` the stage moves there
(`StageOpen`) and `stageCloses += openRounds`. A unit on the old stage stays where it is.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("Only Perform turns essence into score (T296)", "[match][stage]") {
    // a carrier steps onto the stage: score unchanged; Perform: score[side] += essence, essence 0.
}
TEST_CASE("The stage holds one (T297)", "[match][stage]") {
    // an enemy on the stage: Move onto it is illegal; Perform away from it is illegal.
}
TEST_CASE("The stage announces its next room, then moves (T298)", "[match][stage]") {
    // round stageCloses - 1: nextStage set, different from stage, logged; round stageCloses: stage
    // == that room; a unit left on the old room keeps its cell; a monster on the new room blocks entry.
}
```

- [ ] **Step 2: Run** → FAIL. **Step 3: Implement.** **Step 4: Run** all → pass.
- [ ] **Step 5: Commit** `issue #UMBRA-257 The stage and Perform (T296-T298)`.

### Task 8: Monsters: behaviours, guardians, respawn, the great monster (#258)

**Files:**
- Create: `modules/match/src/Monsters.cpp`
- Test: `tests/MatchMonsterTests.cpp`

**Interfaces:**
- Produces: `void RunMonsters(const MatchDef &def, MatchState &s);` (called by Task 4's round advance)

**Rules (spec, Monsters; my calls marked in `Match.md`):** after every round, in unit order:
Stay does nothing; Flee steps one cell away from the nearest character; Stage steps toward the stage
and stands on it when free; Guard hits each enemy character standing next to it with its first
Strike's `Damage`, no battle (it can down them) **[call: guardians do not start battles]**. A unit
cannot attack a monster of its own faction. A dead weak monster returns to its den at
`respawnRound` if the den is free. The great monster appears in a stage room other than the stage at
`great.wakeRound`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("Monsters act after each round by their behaviour (T299)", "[match][monsters]") {
    // a Flee monster's distance to the nearest character grows; a Stage monster reaches a free stage.
}
TEST_CASE("A guardian strikes enemies next to it, and its own side cannot attack it (T300)", "[match][monsters]") {}
TEST_CASE("Weak monsters come back to their dens (T301)", "[match][monsters]") {}
TEST_CASE("The great monster wakes at its round (T302)", "[match][monsters]") {}
```

- [ ] **Step 2: Run** → FAIL. **Step 3: Implement.** **Step 4: Run** all → pass.
- [ ] **Step 5: Commit** `issue #UMBRA-258 Monsters: behaviours, guardians, the great monster (T299-T302)`.

### Task 9: The bots, at three levels (#259)

**Files:**
- Create: `modules/matchbots/CMakeLists.txt`, `include/MatchBots.hpp`, `src/MatchBots.cpp`, `include/Bots.md`
- Test: `tests/MatchBotTests.cpp`

**Interfaces:**
- Produces:
  - `enum class BotRole { Carry, Defend, Guard };`
  - `std::array<BotRole, 8> Roles(const MatchDef &def, const MatchState &s, int faction);` (per character in `order`; Guard if an enemy character is within `strongMin` steps of the hub while the guardian stands; Defend for the one nearest the stage when an enemy carries essence; the rest Carry)
  - `Action ChooseAction(const MatchDef &def, const MatchState &s, BotLevel level);` (for `Current(s)`)

**Approach:** each call scores every reachable cell with the actions possible there, using
`Preview` and the unit's role (carry: monsters by essence per HP spent, then the stage when carrying;
defend: enemy carriers; guard: enemies near home; everyone: go home when HP is under a third). It
adds noise of `bots.noise[level]` percent of the value, from the state's random numbers, and returns
the first step toward the best cell, or its action, or `EndTurn`. In battle: the best affordable
battle ability by `Preview`. All weights are named constants in `Bots.md` and `rules.json` `bots`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("Bots play matches to the end on every seed (T303)", "[match][bots]") {
    // seeds 1-50, both sides bots (Medium): Current reaches -1 within 20000 Applies, every Apply true.
}
TEST_CASE("A carrier with essence heads for the stage and Performs (T304)", "[match][bots]") {
    // a Hard bot carrying 10 essence, 3 steps from a free stage: within its next two turns, Performed.
}
TEST_CASE("A bot that cannot afford anything still acts (T305)", "[match][bots]") {
    // every unit at 1 HP: ChooseAction is legal on every call (battle: the free Strike; board: EndTurn).
}
```

- [ ] **Step 2: Run** → FAIL. **Step 3: Implement.** **Step 4: Run** all → pass.
- [ ] **Step 5: Commit** `issue #UMBRA-259 The bots at three levels (T303-T305)`.

### Task 10: The soak, its tool command and the first balance (#260)

**Files:**
- Create: `modules/matchbots/include/MatchSoak.hpp`, `src/MatchSoak.cpp`, `tool/MatchCommand.cpp`
- Test: `tests/MatchBotTests.cpp` (T306, T307)

**Interfaces:**
- Produces:
  - `struct MatchReport { int runs = 0, faults = 0, draws = 0; std::array<int, 2> wins{}; int firstMoverWins = 0, comebacks = 0, guardianFalls = 0; long long fromMonsters = 0, fromKills = 0, fromGuardians = 0; double decidedRound = 0; bool Holds(const MatchDef &def) const; };` (`fromX`: essence gained by its source; `comebacks`: matches won by the side behind after two thirds of the rounds; `decidedRound`: mean of the last round the lead changed)
  - `MatchReport SoakMatches(const MatchDef &def, std::uint64_t firstSeed, int runs, BotLevel side0, BotLevel side1);`
  - `std::string Text(const MatchDef &def, const MatchReport &r);`
  - `UMBRA_TOOL_COMMAND(MatchSoak, "match-soak", "match-soak [--data <folder>] [--runs 200] [--seed 1] [--levels medium,medium]", ...)`

**Holds** (from `rules.json` `balance`): no faults; each side wins 45-55%; the side that moves first
in `order` wins no more than 5 points over half; no source over 60% of gained essence; comebacks at
least 25%.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("The soak counts sources and comebacks (T306)", "[match][bots]") {
    // 20 runs: runs == 20, faults == 0, wins + draws == 20, fromMonsters > 0, decidedRound in 1..30.
}
TEST_CASE("The faction match is in balance (T307)", "[match-balance]") {
    // 400 runs, Medium vs Medium: CHECK(report.Holds(def)), printing Text(def, report) on failure.
}
```

- [ ] **Step 2: Run** `UmbraMatchTests "[bots]"` → FAIL.
- [ ] **Step 3: Implement** the soak and the command.
- [ ] **Step 4: Run** `UmbraMatchTests` → pass; run `UmbraTool match-soak --runs 400` and tune the data only (no rule changes) until `[match-balance]` holds. If a band cannot be reached by data alone, stop tuning, record the numbers in `Bots.md` and the ledger, and report to the user (the band may be wrong, or a rule).
- [ ] **Step 5: Run** `bash /home/claude/verify.sh` → Expected: GCC, Clang and ASan all pass, old and new suites.
- [ ] **Step 6: Commit** `issue #UMBRA-260 The soak, match-soak and the first balance (T306-T307)`; update `docs/current-engine-status.md` and a `docs/testing/` record (headless only; no Windows or GPU claim).
