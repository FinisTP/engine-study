# Faction matches, Plan 2: the screens — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `LastCurtain.exe` plays a 3v3 faction match: pick your three, play against a bot faction on
the drawn board with the five cast sprites and the specter, battles on their own screen, a forecast
before every action, and a reason on everything you cannot do; saved on quit.

**Architecture:** The night's pattern, for the match. `MatchPlay` (headless, tested) holds a
`MatchState`, takes the player's input as `Action`s, paces the bots one beat at a time, and gives each
screen its JSON. `MatchTable` draws the board and the sprites. `MatchApp.cpp` is the loop;
`Main.cpp` runs it unless `--night` asks for the old night (kept until Plan 3 removes it). Rules stay in
`modules/match`; nothing here decides anything the rules decide.

**Tech Stack:** C++20, engine/ui (`*.ui.json`), the renderer and sprite shader already used by the
board, nlohmann_json, Catch2.

**Spec:** `docs/design/2026-11-19-factions.md` (and Plan 1's `Match.md`, `Board.md`, `Bots.md`).

**Not here (Plan 3):** the 1v1 tutorial, special effects (skip, push last, the follow-up passive),
more item classes, sounds for the match, retiring the night and `--night`.

## Global Constraints

- Display never decides: every rule question goes to `Check`, `Legal`, `Apply`, `Preview`.
- The user's design rule: anything the player cannot use shows why (`Why` → words in
  `assets/match-words.json`), as a tooltip on hover and as a short line on the disabled button.
- Short code with `// 1.` comments; markdown beside code for anything deeper; ASCII C++ literals.
- Screen text and timings are data (`assets/ui/match-*.ui.json`, `assets/match-words.json`,
  `assets/options.csv`), not C++ strings.
- Characters face right; a figure on side 1 is mirrored to face left (`SpriteState` facing).
- Never report a GPU or Windows check as passed from Linux; SwiftShader captures are evidence of
  drawing, not of the look on the user's card.
- Commits `issue #UMBRA-<n>`, #263 to #270, with the session's two closing lines. Tests from T308.

## The screens (calls of mine, marked for the user's review)

```
 ┌───────────────────────────────────────────────────────────────────────────┐
 │ SIDE A  24 ▮▮▮▮▮▯▯ 17  SIDE B        27 turns left    Stage: closes in 3  │  top bar
 │ [Lan][Bao][Huy][Mai*][Khang][Lan]   (turn order, side colours, * acting)  │
 │                                                                           │
 │                 the board: rooms, halls, the stage glowing,               │
 │                 the next stage pulsing, figures with side rings,          │
 │                 essence over carriers ("12"), items on the floor          │
 │                                                                           │
 │ ┌─ Mai (Supporter) ─────────┐            ┌─ Forecast ──────────────────┐  │
 │ │ HP 14/20  Essence 6       │            │ Tend on Khang: +6 HP, -3 HP │  │
 │ │ Moves left 2  [Undo]      │            └─────────────────────────────┘  │
 │ │ [Attack] [Tend 3] [Perform: not on the stage]                         │  │
 │ │ [Bandage] [Pick up] [End turn]                                        │  │
 │ └───────────────────────────┘                                             │
 └───────────────────────────────────────────────────────────────────────────┘
```

- **Pick** (before a match): the five cast cards (portrait, role, five stats, starting abilities);
  click three; bot level (easy, medium, hard); Start. Continue if a save exists.
- **Board (your turn):** click a square to walk there (the path shows, red past your moves), or
  arrows to step; Undo; click an enemy next to you to attack; pick a board ability then its target;
  the forecast panel follows the hovered target. Disabled buttons carry their reason.
- **Battle** (its own screen, over a dimmed board): attacker left, defender right, sprites facing each
  other with their motions; HP bars and shields; "Round 2 of 3"; on your pick, your battle abilities
  with their forecasts; a line for each blow. Bots' picks show one beat each.
- **Level up:** the two abilities side by side; click one.
- **Event:** its name and text, two choices with their chance; the result line after.
- **Bot turns:** a caption per beat ("Lan carries 12 essence to the stage"), Continue or Skip, as the
  night's captions; moves slide.
- **End:** the winner, both scores, kills, essence performed, guardians fallen; New match.

---

### Task 1: Saving and loading a match (#263)

**Files:** Create `modules/match/include/MatchSave.hpp`, `modules/match/src/MatchSave.cpp`; Test `tests/MatchSaveTests.cpp`.

**Interfaces — produces:**
- `nlohmann::json SaveMatch(const MatchState &s);` (`"format": "umbra.match", "version": 1`; every field of `MatchState`, the board included)
- `bool LoadMatch(const nlohmann::json &j, MatchState &out, std::string &error);` (refuses another format or version, and a malformed file, with a reason)
- `std::uint64_t Fingerprint(const MatchState &s);` (a hash of every field but the log, for tests)

- [ ] **Step 1: failing tests** — T308 "A saved match loads to the same state": play 200 bot actions on seed 3, save, load, `Fingerprint` equal, and the next 200 bot actions give the same fingerprints on both. T309 "A save of another version or a broken file is refused": version 2 → false with "version"; a cut-off file → false; `out` untouched.
- [ ] **Step 2: run** `UmbraMatchTests "[save]"` → FAIL (no MatchSave.hpp).
- [ ] **Step 3: implement** with nlohmann_json, arrays for vectors.
- [ ] **Step 4: run** → pass. **Step 5: commit** `issue #UMBRA-263 Saving and loading a faction match (T308-T309)`.

### Task 2: MatchPlay: the player's actions, the bots' beats, the HUD (#264)

**Files:** Create `apps/LastCurtain/MatchPlay.hpp`, `MatchPlay.cpp`, `MatchPlay.md`, `assets/match-words.json`; Modify `apps/LastCurtain/CMakeLists.txt` (add to `UmbraNightPlay`; link `Umbra::Match`, `Umbra::MatchBots`); Test `tests/MatchPlayTests.cpp` (in `UmbraMatchTests`, linking `Umbra::NightPlay`).

**Interfaces — produces:**
- `class MatchPlay` with `MatchPlay(const MatchDef &def, std::uint64_t seed, std::vector<int> picks, BotLevel bots)` and `MatchPlay(const MatchDef &def, MatchState saved, BotLevel bots)`.
- `bool Do(const Action &a)` (the player's action; false with no change if `Check` refuses), `void Update(float dt)`, `bool PlayerTurn() const` (`Current` is on side 0), `std::string Screen() const` (`""` board, `"caption"`, `"battle"`, `"level"`, `"event"`, `"end"`), `nlohmann::json UiData() const`, `bool HandleUi(const std::string &screen, const std::string &action, const std::string &arg)`, `const MatchState &State() const`, `nlohmann::json Save() const`.
- `struct Beat { std::vector<LogEntry> entries; std::string caption; };` and `const Beat *Showing() const` (the bot beat on screen).
- Words: `match-words.json` holds `why` (one line per `Why`), `log` (a caption template per `LogKind`, with `{a}`, `{b}`, `{c}` names and numbers), `roles`, `stats`.

**Behaviour:** the player's units (side 0) wait for `Do`; on a bot unit's turn, `Update` asks
`ChooseAction` (level from the constructor), applies it, and holds the screen for the beat:
`options.csv` `beatSeconds` for a deed (battle, ability, perform, event, down), `plainBeatSeconds`
for a step; with `tapToContinue`, a deed waits for Continue. Skip runs bots to the player's turn.
`UiData()["hud"]` holds: scores, `TurnsLeft`, the stage (closes in N, next room once announced), the
turn order (name, side, acting, down), the acting unit (HP, essence, moves left, items) and
`buttons`: one per action kind on offer, each `{label, action, arg, enabled, why}` from `Check`.

- [ ] **Step 1: failing tests** — T310 "Bots play until it is the player's turn, one beat at a time": a match where order[0] is a bot; `Update` with no time passes no beat; after `beatSeconds` one beat; the player's turn stops the bots. T311 "The HUD's buttons carry the rules' reasons": on the player's turn away from the stage, the Perform button is `enabled: false` with `why` = `match-words.json` `why.NotOnStage`; End turn enabled. T312 "A refused action changes nothing": `Do(Perform)` off the stage returns false and the fingerprint is unchanged.
- [ ] **Step 2: run** → FAIL. **Step 3: implement.** **Step 4: run** → pass. **Step 5: commit** `issue #UMBRA-264 MatchPlay: the player's actions, the bots' beats, the HUD (T310-T312)`.

### Task 3: Walking, targeting and the forecast (#265)

**Files:** Modify `MatchPlay.hpp/.cpp`; Test `tests/MatchPlayTests.cpp`.

**Interfaces — produces:** `void Hover(int cell)`, `void Click(int cell)` (walk there along the shortest legal path within moves, or attack the enemy there if adjacent, or the target of the armed ability), `void Arm(int abilitySlot)` / `Disarm()`, `struct Walk { std::vector<int> path; int affordable; }` and `Walk Previewed() const`, `nlohmann::json UiData()["forecast"]` (`Preview` for the hovered target and armed ability, or the attack's first exchange), `struct Slide { int unit, from, to; float t; }` and `Slide Sliding() const` (a step plays over `slideSeconds`).

**Calls:** the attack forecast shows the player's best free Strike against the foe and the foe's
against the player for one exchange, labelled "first blow" (Plan 1's deferred minor: `Preview` gives
nothing for Attack; this is the display's own estimate from `Damage`, no rule).

- [ ] **Step 1: failing tests** — T313 "Clicking a square walks the shortest legal path": the unit ends on the clicked square, one `Moved` entry per step; a square past the moves is not walked and `Previewed().affordable` is the reachable part. T314 "An armed ability targets by click and shows its forecast": arm Tend, hover an ally, `forecast` = `Preview`; click → applied; hover an enemy → `why: OwnSide`. T315 "Clicking an adjacent enemy starts a battle; a far one does nothing".
- [ ] **Steps 2-5** as above; commit `issue #UMBRA-265 Walking, targeting and the forecast (T313-T315)`.

### Task 4: Battle, level-up, event and end screens' data (#266)

**Files:** Modify `MatchPlay.cpp`; Create `assets/ui/match-battle.ui.json`, `match-level.ui.json`, `match-event.ui.json`, `match-end.ui.json`; Test `tests/MatchPlayTests.cpp`.

**Behaviour:** `Screen()` is `"battle"` while `battle.on`: `UiData()["battle"]` holds both fighters
(name, sprite, side, HP, max HP, shield), the round of `battle.rounds`, the last blows, and (on the
player's pick) its battle abilities with forecast and `why`. `"level"` while a player unit has
`pending`: the two abilities (name, class, cost, power or dice, range). `"event"` when the player
stands on an unresolved event and chooses Answer from the HUD: name, text, two choices with chance;
after the answer, the result line, then Continue. `"end"` when over: winner, scores, kills, essence
performed per side, guardians fallen; New match.

- [ ] **Step 1: failing tests** — T316 "The battle screen follows the rules' battle": after an attack, `Screen()=="battle"`, both HP match the state, the player's buttons are its battle abilities; a bot defender's pick shows as one beat. T317 "Level choices and events come as their own screens" (`LevelUp` via `HandleUi("level", "choose", "1")`; an event answered via `HandleUi("event", "choose", "0")`). T318 "The end screen's numbers come from the state".
- [ ] **Steps 2-5**; `UmbraTool ui-preview` on each new screen with `assets/ui/sample.json` additions renders without errors; commit `issue #UMBRA-266 Battle, level-up, event and end screens (T316-T318)`.

### Task 5: The pick screen, the HUD screen, and Continue (#267)

**Files:** Create `assets/ui/match-pick.ui.json`, `match-hud.ui.json`, `match-forecast.ui.json`; Modify `MatchPlay.hpp/.cpp` (`static nlohmann::json PickData(const MatchDef &def, const std::vector<int> &chosen, BotLevel level, bool canContinue)`); Test `tests/MatchPlayTests.cpp`.

**Behaviour:** the pick screen's cards come from `characters.csv` and `jobs.csv` (level-1 abilities),
the portrait `art/sprites/<sprite>/face_calm.png`; Start is enabled at exactly `options.side` picks,
else it says why ("Pick 3"). The HUD screen lays out `UiData()["hud"]` as in the sketch; buttons
show `why` under a disabled label and as their tooltip.

- [ ] **Step 1: failing test** — T319 "The pick screen allows a start only with a full side": 2 picks → Start disabled with "Pick 3"; 3 → enabled; a 4th click replaces nothing (refused, says why).
- [ ] **Steps 2-5**; `ui-preview` of `match-pick` and `match-hud`; commit `issue #UMBRA-267 The pick and HUD screens (T319)`.

### Task 6: MatchTable: the board and its figures (#268)

**Files:** Create `apps/LastCurtain/MatchTable.hpp`, `MatchTable.cpp`, `MatchTable.md`; Modify `Motion.hpp/.cpp` (`std::vector<std::string> MotionsFor(const umbra::match::LogEntry &e)` beside the night's).

**Interfaces — produces:** `MatchTable(Renderer&, const AssetFolders&)`, `void LoadArt(const MatchDef&, const AssetFolders&)`, `void Draw(const MatchDef&, const MatchState&, const MatchPlay::Walk&, const MatchPlay::Slide&, int acting, DrawList&) const`, `void React(const std::vector<LogEntry>&)` (motions: attack, hit, down, perform = a bow, level = a hop), `void Update(float dt)`, `int Pick(glm::vec2 mouse, glm::vec2 pixels) const` (the square under the mouse), camera calls as `Table`.

**Look (calls):** squares are low tiles: halls dark wood, rooms faded carpet, the centre the Opera's
red, hubs in their side's colour; doors are gaps in a thin wall rim. The stage square glows warm
gold; the next stage pulses until it opens. Figures stand on their square, 1.6 squares tall for
characters, 1.2 for weak monsters, 2.0 for strong and guardians, 3.0 for the great monster; side 0
faces right, side 1 mirrored; a ring under each in its side's colour (neutral: none); a carrier's
essence floats as a number above it. Monsters use the specter sprite with `monsters.csv` `tint`.

- [ ] **Step 1: check by picture** (no unit test for drawing): `LastCurtain --match --seed 5 --watch --frames 240 --capture match.png` under SwiftShader shows the board, all twelve figures, the stage glow. Compare with the sketch; fix and retake until it reads.
- [ ] **Step 2: commit** `issue #UMBRA-268 MatchTable: the faction board and its figures`.

### Task 7: The loop: MatchApp, and Main's switch (#269)

**Files:** Create `apps/LastCurtain/MatchApp.cpp`, `MatchApp.hpp` (`int RunMatch(AppContext&)`); Modify `Main.cpp` (shared setup into an `AppContext`; `--night` runs the old night, otherwise the match), `Options` (`--match` flags for headless runs: `--seed`, `--watch`, `--frames`, `--capture`, `--picks bao,mai,lan`).

**Behaviour:** pick screen → match → end → pick; Esc opens the pause menu (Resume, Save and quit);
quitting saves to `saves/match.json` and Continue loads it; the board takes clicks only when no
dialog has them; `--watch` plays both sides by bots for captures.

- [ ] **Step 1: check by picture:** captures of the pick screen, a player turn with the forecast, a battle, the end screen (four PNGs) under SwiftShader. **Step 2:** the GCC/Clang/ASan suites pass. **Step 3: commit** `issue #UMBRA-269 MatchApp: the faction match runs in LastCurtain`.

### Task 8: The record and the Windows check (#270)

**Files:** `docs/testing/s41-faction-match-screens.md` (§4: the user's checklist on Windows: pick, a full match against Medium, a battle, a level choice, an event, Continue after quit, the look of the board and sprites, frame time), `docs/current-engine-status.md`, `docs/ROADMAP.md` (S41 row), `apps/LastCurtain/README.md` (how to play).

- [ ] **Step 1:** write them from the session's evidence only. **Step 2: commit** `issue #UMBRA-270 Faction match screens: the record and the Windows checklist`.

## Review Focus

1. Input during a bot beat (a click while the bots play) must be ignored or queued, never applied to the bot's unit → T310 asserts `Do` refuses when `!PlayerTurn()`.
2. A player unit downed in its own battle: the screen leaves the battle, the turn passes, no stale buttons → T316 includes it.
3. A saved match in mid-battle or mid-level-choice loads to the same screen → T308 saves at a battle pick.
4. Hovering off the board or onto a wall gives no path and no forecast, without error → T313.
5. Every `Why` value has a word in `match-words.json` → T311 checks all of them.
