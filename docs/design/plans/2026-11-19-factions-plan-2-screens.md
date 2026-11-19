# Faction matches, Plan 2: the centre's monsters, then the screens — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** First the rules the user changed on 2026-11-20 (a fixed stage, a centre held by aggressive
monsters that start battles, spawns that grow stronger, monsters that fight with abilities and can
be 2x2 or 3x3); then `LastCurtain.exe` plays the match with the screens of the approved prototype
(https://claude.ai/artifact/4LB1dit1jm2niE3gz8uFLw).

**Architecture:** Part A changes `modules/match` and `modules/matchbots` only, test first, and
re-balances with the soak. Part B is the night's pattern for the match: `MatchPlay` (headless,
tested) turns input into `Action`s, paces bots and monsters, and gives each screen its JSON;
`MatchTable` draws; `MatchApp.cpp` is the loop; `Main.cpp` runs it unless `--night`.

**Tech Stack:** C++20, engine/ui (`*.ui.json`), the renderer and sprite shader the board already uses,
nlohmann_json, Catch2.

**Spec:** `docs/design/2026-11-19-factions.md` (updated 2026-11-20); the prototype above is the
screens' reference.

**Not here (Plan 3):** the 1v1 tutorial, special effects (skip, push last, the follow-up passive),
more item classes, sounds, retiring the night and `--night`.

## Global Constraints

- Display never decides: every rule question goes to `Check`, `Legal`, `Apply`, `Preview`.
- The user's design rule: anything the player cannot use says why (`Why` → `assets/match-words.json`),
  as a tooltip on hover.
- Every number is data (`assets/match/*.csv`, `rules.json`, `assets/options.csv`); screen text and
  timings are data (`assets/ui/match-*.ui.json`, `assets/match-words.json`).
- Short code with `// 1.` comments; markdown beside code for deeper ideas; ASCII C++ literals.
- Mirrored fairness: anything placed or spawned in one half appears in the other at the same time.
- Never report a GPU or Windows check as passed from Linux.
- Commits `issue #UMBRA-<n>`, #264 to #275, with the session's closing lines. Tests from T310.

## Review Focus

1. A hunting monster's battle against a player character in the monsters' phase must wait for the
   player's picks and then let the phase go on → T318.
2. A 2x2 or 3x3 monster at a board edge or next to the stage: it never covers a wall, the stage or
   another unit → T315.
3. A spawn due on a square someone stands on: it waits (still announced) and appears when free → T313.
4. A save taken mid monster phase loads to the same moment → T314.
5. Every `Why` and every log kind has words → T322.

---

## Part A: the rules

### Task 1: Saving and loading a match (#263) — done

### Task 2: The new data (#264)

**Files:** Modify `MatchDef.hpp`, `MatchData.cpp`, `Match.md`; `assets/match/abilities.csv` (column
`target`: enemy, ally, self, monster, room, any), `monsters.csv` (columns `level`, `size` 1-3, `sight`;
behaviour `hunt`; column `respawnRounds` removed), new `spawns.csv` (`zone,from,monster,every`: zone
weak, strong or centre; from = first round; every = rounds after the square empties), `rules.json`
(`spawn.warn` = rounds of warning, default 1; `stage` removed); Test `tests/MatchDataTests.cpp`.

**Interfaces:** `enum class Target { Enemy, Ally, Self, Monster, Room, Any }` on `AbilityDef::target`;
`MonsterDef::level, size, sight`; `Behaviour::Hunt`; `struct SpawnRow { int zone, from, monster, every; }`
and `MatchDef::spawns`; `Rules::spawn.warn`.

**Data (first set):** centre kinds `usherette` (hunt, level 2, sight 3), `ghost-chorus` (hunt, level
4, 2x2, from round 12), `the-spider` (hunt, level 6, 3x3, from round 20, worth the most); lane kinds
as now; each with 1-3 battle abilities, some shields or heals.

- [ ] **Step 1: failing tests** — T310 "The new columns load": `target`, `level`, `size`, `sight`, the
  spawn rows sorted by zone and round. T311 "Bad spawn or size data is refused": size 4, an unknown
  zone, a spawn of a guardian, `every` 0.
- [ ] **Steps 2-4:** red, implement, green (old tests updated where a column moved).
- [ ] **Step 5: commit** `issue #UMBRA-264 The data for spawns, monster levels and sizes, ability targets (T310-T311)`.

### Task 3: A fixed stage and spawns (#265)

**Files:** Modify `Board.cpp` (spawn squares: `Board::centreSpawns`, mirrored, never the stage),
`Match.cpp`, `Stage.cpp` (the moving stage removed), `Monsters.cpp`, `MatchSave.cpp` (version 2);
Test `tests/MatchSpawnTests.cpp`, update `MatchStageTests.cpp`.

**Interfaces:** `MatchState::stage` fixed at the centre of the middle row; `stageCloses`, `nextStage`
removed; `struct Spawn { std::int16_t cell; std::int8_t monster; int round; }` and
`MatchState::spawns` (announced spawns); `LogKind::SpawnSoon`, `LogKind::Spawned`.

**Rules:** at the start, every spawn square gets the kind its zone's latest row at round 1 names.
A square that empties schedules the next spawn `every` rounds later, of the kind the zone's latest
row names at that round; `spawn.warn` rounds before, it is announced (`SpawnSoon`, listed in
`spawns`). It appears at the start of its round, or as soon as its squares are free. Mirrored squares
spawn together. Killed monsters never return.

- [ ] **Step 1: failing tests** — T312 "The stage stays put and monsters never stand on it". T313
  "An emptied square announces its next spawn and fills it, waiting while occupied" (kind follows the
  round's row; mirror spawns together). T314 "Spawns survive a save" (save version 2; version 1 refused).
- [ ] **Steps 2-5**; commit `issue #UMBRA-265 A fixed stage; monsters spawn instead of returning (T312-T314)`.

### Task 4: Large monsters (#266)

**Files:** Modify `Match.hpp` (`bool Covers(const MatchDef&, const Unit&, int cell)`,
`std::vector<int> Squares(const MatchDef&, const Unit&)`), `Turn.cpp`, `Battle.cpp`, `Abilities.cpp`,
`Monsters.cpp`; Test `tests/MatchMonsterTests.cpp`.

**Rules:** a unit of size s stands on its anchor square (top-left) and covers s x s squares; it is
placed, spawned or moved only where every covered square is open, free and not the stage; a square
it covers counts as occupied; "next to" and range count from its nearest covered square.

- [ ] **Step 1: failing tests** — T315 "A 3x3 monster covers nine squares, never a wall, the stage or
  another unit". T316 "Anyone next to any of its squares can attack it, and its blows reach from any".
- [ ] **Steps 2-5**; commit `issue #UMBRA-266 Monsters of 2x2 and 3x3 squares (T315-T316)`.

### Task 5: Hunters and the monsters' phase (#267)

**Files:** Modify `Match.hpp` (`MatchState::monsterTurn`, -1 outside the phase), `Monsters.cpp`,
`Turn.cpp`, `Battle.cpp`; Create `modules/match/src/MonsterMind.cpp` (`int MonsterPick(const MatchDef&,
const MatchState&, int unit)`: the battle ability a monster uses, by expected value: damage, a shield
when the foe could down it, a heal below half HP); Test `tests/MatchMonsterTests.cpp`.

**Rules:** after the last character, monsters act one by one (`monsterTurn`). A hunter that sees a
character within `sight` steps walks toward it up to `speed` steps without leaving the centre
squares, and next to it starts a battle as attacker. The battle pauses the phase: `Current` is the
character when its side must pick; after the battle the phase goes on, then the round ends. Every
monster's battle picks use `MonsterPick` (the guardians' blows stay as they are).

- [ ] **Step 1: failing tests** — T317 "A hunter walks to a character it sees, never out of the centre".
  T318 "A hunter's battle waits for the defender's picks, then the monsters' phase goes on". T319
  "Monsters pick shields and heals when they help" (a hurt monster with a heal uses it; one about to
  be downed by its foe's next blow shields).
- [ ] **Steps 2-5**; commit `issue #UMBRA-267 Hunting monsters start battles; monsters pick abilities (T317-T319)`.

### Task 6: Bots and the balance (#268)

**Files:** Modify `MatchBots.cpp` (the danger of standing in a hunter's sight; ability targets),
`Bots.md`; `assets/match/spawns.csv`, `monsters.csv` (tuning only); Test `tests/MatchBotTests.cpp`.

- [ ] **Step 1: failing test** — T320 "A bot does not walk a hurt character into a hunter's sight"
  (HP under a third, a hunter 2 steps off: the bot's square stays out of its sight).
- [ ] **Step 2:** implement; T303-T306 still pass.
- [ ] **Step 3:** soak 400 and tune the data (spawns, monster numbers) until `[match-balance]` holds;
  T321 "Late centre monsters are worth more essence than early ones" (the mean essence of monsters
  downed after round 20 exceeds that before round 10). If the bands cannot hold by data, stop, record,
  ask the user.
- [ ] **Step 4: commit** `issue #UMBRA-268 Bots mind the hunters; the new centre is in balance (T320-T321)`.

## Part B: the screens (as the prototype)

### Task 7: MatchPlay: beats, the turn banner, the order and the log (#269)

**Files:** Create `apps/LastCurtain/MatchPlay.hpp`, `MatchPlay.cpp`, `MatchPlay.md`, `assets/match-words.json`;
Modify `apps/LastCurtain/CMakeLists.txt`; Test `tests/MatchPlayTests.cpp`.

**Interfaces:** `MatchPlay(const MatchDef&, std::uint64_t seed, std::vector<int> picks, BotLevel)`,
`MatchPlay(const MatchDef&, MatchState saved, BotLevel)`; `bool Do(const Action&)`, `void Update(float)`,
`bool PlayerTurn() const`, `std::string Screen() const` (`""`, `"banner"`, `"battle"`, `"level"`,
`"event"`, `"end"`), `nlohmann::json UiData() const`, `bool HandleUi(screen, action, arg)`,
`const MatchState &State() const`, `nlohmann::json Save() const`.

**Behaviour:** each new round opens with the banner "Turn N" and "M turns left" (`bannerSeconds`).
Bots and monsters play one beat at a time (`beatSeconds`, `plainBeatSeconds`; Skip runs to the player).
`UiData()` holds `score`, `order` (continuous: from the acting unit to the end of the round, a
`next turn` marker, then those who have acted; downed ones dimmed), `unit` (the acting or selected
character: portrait, full name, role, level, HP, essence), `log` (every caption, newest last;
`logOpen`).

- [ ] **Step 1: failing tests** — T322 "Every log kind and every Why has words". T323 "The order strip
  runs on past the round's end" (after the first acts, it heads the list again after the marker). T324
  "Bots and monsters play one beat at a time and stop at the player's turn; input is refused meanwhile".
- [ ] **Steps 2-5**; commit `issue #UMBRA-269 MatchPlay: beats, banner, turn order and log (T322-T324)`.

### Task 8: The turn: move range, actions, targets and the forecast (#270)

**Files:** Modify `MatchPlay.hpp/.cpp`; Test `tests/MatchPlayTests.cpp`.

**Behaviour (the prototype's flow):** at the turn's start `UiData()["reach"]` lists every square the
unit can reach (and the path to the hovered one); clicking one walks there, clicking the unit stays.
Then `actions`: Undo move (or Back before moving), Attack, each board ability, Item, Pick up, Answer,
Perform (only where they apply), End turn; each `{id, label, note, enabled, why}` from `Check`.
Choosing Attack or an ability arms it: `targets` lists valid squares (ability `range` shown as
`reach`), the unit panel switches to the unit's HP, Attack, Defence. `forecast` follows the hovered
square: a target's HP, ATK, DEF, essence and level with the first blow or the ability's `Preview`; the
stage (one at a time, perform here); an announced spawn (what, level, which round); any unit's numbers.

- [ ] **Step 1: failing tests** — T325 "The move range and path match the rules; staying is a click on
  oneself". T326 "Actions offer only what applies, each refusal with its reason" (no one adjacent →
  Attack off with `why.NoTarget`; Perform only on the stage). T327 "The forecast covers targets, the
  stage, spawns and units".
- [ ] **Steps 2-5**; commit `issue #UMBRA-270 The turn's flow: range, actions, targets, forecast (T325-T327)`.

### Task 9: Battle, level-up, event and end (#271)

**Behaviour:** `"battle"`: both fighters (portrait, name, level, HP, shield, sprite, side), `Round r of 3`,
the last blow's line, and on the player's pick its battle abilities with damage and cost (or why not).
A monster's or bot's pick is one beat. `"level"`: the two abilities. `"event"`: name, text, two choices
with chance, then the result. `"end"`: winner, scores, kills, essence performed, monsters downed.

- [ ] **Step 1: failing tests** — T328 "The battle screen follows the rules' battle, a hunter's
  included". T329 "Level choices and events are their own screens". T330 "The end screen's numbers come
  from the state".
- [ ] **Steps 2-5**; commit `issue #UMBRA-271 Battle, level-up, event and end screens (T328-T330)`.

### Task 10: The screen files (#272)

**Files:** `assets/ui/match-pick.ui.json` (five cards, pick three, bot level, Start, Continue),
`match-hud.ui.json` (score top left, order strip top centre, unit panel bottom left, action list left,
forecast right, log bottom right folding to a button, hint bottom centre), `match-banner`,
`match-battle`, `match-level`, `match-event`, `match-end`; `assets/ui/sample.json` entries.

- [ ] **Step 1: failing test** — T331 "The pick screen starts only with a full side".
- [ ] **Step 2:** `UmbraTool ui-preview` renders each file without errors. **Step 3: commit**
  `issue #UMBRA-272 The match's screen files (T331)`.

### Task 11: MatchTable: the board and its world UI (#273)

**Files:** Create `apps/LastCurtain/MatchTable.hpp`, `MatchTable.cpp`, `MatchTable.md`; Modify `Motion.cpp`
(`MotionsFor(const umbra::match::LogEntry&)`).

**Look (the prototype):** tiles by kind (halls dark wood, rooms faded carpet, the centre opera red,
hubs in side colours), the stage glowing gold, announced spawn squares marked; the reach in blue (a
bot's in orange), targets red, an ability's range orange, the hovered square outlined. Figures on
their squares, side rings; a large monster centred on its 2x2 or 3x3 footprint. Above each unit: the
gold arrow for the acting one, then a card with portrait, name, level and HP bar, and the carried
essence as an outlined number.

- [ ] **Step 1: check by picture:** `LastCurtain --seed 5 --watch --frames 240 --capture match.png`
  under SwiftShader shows all of it. **Step 2: commit** `issue #UMBRA-273 MatchTable: board and world UI`.

### Task 12: The loop (#274)

**Files:** Create `apps/LastCurtain/MatchApp.cpp`, `MatchApp.hpp`; Modify `Main.cpp` (`--night` keeps the
old night; otherwise the match), `Options`.

- [ ] **Step 1:** pick → match → end → pick; Esc pause (Resume, Save and quit); Continue loads
  `saves/match.json`. **Step 2: check by picture:** pick screen, a turn with the forecast, a hunter's
  battle, the end. **Step 3:** GCC, Clang, ASan suites. **Step 4: commit** `issue #UMBRA-274 MatchApp: the faction match runs in LastCurtain`.

### Task 13: The record and the Windows checklist (#275)

`docs/testing/s41-faction-match-screens.md` (§4 the user's Windows checks), status, roadmap, README;
commit `issue #UMBRA-275 Faction match screens: the record and the Windows checklist`.
