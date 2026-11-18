# The performances, plan 1: the rules — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The night becomes a programme of acts: guests earn Applause on the board and in a solo duel
against each act's specter, using the props its script calls for, and the most Applause at dawn wins.

**Architecture:** New data (`scripts.csv`, `specters.csv`, `rules.json` `acts`/`duel`, `items.csv`
`prop`/`copies`) loads into `NightDef`. The act and the duel live in a new `Acts.cpp` of the night
module and in `NightState` (`act`, `scripts`, `Duel`), driven by the same `LegalActions`/`Apply` as
every other turn, so bots, saves and the soak see them like any action. The app gets only a minimal
duel menu and ending here; plan 2 builds the real screens.

**Tech Stack:** C++20, Catch2, nlohmann::json, the night module's CSV reader (`Csv.hpp`).

**Spec:** `docs/design/2026-11-17-performances.md`; the evaluation `docs/design/2026-11-17-performances-eval.md`.

**Plans after this one:** plan 2, the screens (programme, duel view with the sprites and
`motions.json`, the review, Applause in the turn order); plan 3, the roster of 10 and guests per
night (guests become a mapping from `NightState` slots to `guests.csv` rows, a change through the
whole code).

## Global Constraints

- All gameplay data in JSON/CSV under `plugins/last-curtain/assets/night/` (the user).
- Short code, `// 1.` step comments, ASCII C++ literals; a markdown file for anything a comment cannot hold (CLAUDE.md).
- The rules never read display code; display never changes the rules (the user).
- Commits: `issue #UMBRA-<n> <summary>` with the session's Co-Authored-By and Claude-Session lines; from #UMBRA-247.
- Tests continue at T274; a changed rule or data file updates the golden fingerprint (`SaveTests.cpp`, `PlayTests.cpp`).
- Keep the concept count low: Applause, the script, the performance (spec "Three things").
- The Saigon Opera House keeps its name; no *Betrayal at House on the Hill* text.
- Never commit `projects/reference/assets/scenes/settlement.scene.json`; stage by name on the PC.

## Decisions this plan makes (the spec leaves them to the builder; each is a [call] to report)

- **Acts:** rounds 3, 6, 9, 12. An act starts when its round's last turn ends, before the next
  round's start (so the killer and monsters wait: the spec's "the killer pauses during acts").
- **Scripts:** `scripts.csv` has an `act` column; each act draws one script among those of its number.
- **The duel's three moves** (specter shows one before each exchange, drawn from its `moves` list):
  - *Strike:* d20 + Might against the specter's `defense`; a pass deals `2 + Might / 2` damage. Then
    the blow lands, except a light blow (struck first, it flinches: no damage).
  - *Hold:* no damage dealt; d20 + Sanity against `defense`: a pass halves a heavy or light blow and
    stops a taunt.
  - *Use a prop:* one of this act's script props, once each: counts `perProp`; if its cue (`blade:light`)
    is the shown move, the blow misses and it counts `onCue` more. The prop stays carried.
- **Blows:** heavy, light and taunt deal the specter's `heavy`, `light`, `taunt` damage. The specter's
  `hp` caps the damage that counts; at 0 the duel ends at once, standing.
- **Applause of a duel:** `base` if standing at the end + `perDamage` x damage + `perProp` x props +
  `onCue` x cues. Booed off (HP 0): HP becomes 1, the duel ends, `base` is lost, the rest is kept.
- **Board Applause:** a first visit to a room 1, an event passed 2, a monster beaten 3 (`rules.json`).
- **Kill share:** 50% of the victim's Applause (rounded down) moves to the killer (a guest); the house takes nothing.
- **Winner at dawn:** most Applause; ties go to the living, then the higher level, then the earlier in the turn order.
- **Ghosts:** stay where they fell; move, pick up and use a relic, and end their turn; never attack or
  are attacked. Revive on entering the Dressing room (`dressing`) or by a relic (theirs, or one a living
  guest in the room uses on them), at 50% of max HP rounded up.
- **Props on the floor:** when an act is announced, each script prop lies in `copies` random opened
  rooms (not the foyer, not the Dressing room, not a guest's room). Props are never drawn from item
  rooms nor laid by goals.
- **Quests** are removed (their screens too); guest powers stay in the code, unreached, until plan 2
  decides props with powers (the evaluation: "postpone").

## Review Focus

1. A guest killed or booed off mid-act, or every guest a ghost at curtain: the act must still end and the night go on (no stall in `Current`).
2. A save taken during a duel, between exchanges: load must resume the same shown move and performer.
3. The last act at round 12: dawn must follow the finale's last duel, not a 13th round.
4. A prop both scripted and already carried (a guest kept act I's blade into act III): it counts once per act, and copies are not laid again while one is carried.
5. Two guests tied on Applause, one a ghost at dawn: the living one wins.

---

### Task 1: The data — acts, scripts, specters, props

**Files:**
- Modify: `plugins/last-curtain/modules/night/include/NightDef.hpp` (Rules `acts`, `duel`, ghost revive; `ScriptDef`, `SpecterDef`, `PropCue`; `ItemDef::prop`, `copies`; `NightDef::scripts`, `specters`)
- Modify: `plugins/last-curtain/modules/night/src/NightData.cpp` (read and check them)
- Create: `plugins/last-curtain/assets/night/scripts.csv`, `specters.csv`
- Modify: `plugins/last-curtain/assets/night/rules.json`, `items.csv`
- Modify: `plugins/last-curtain/modules/night/include/Night.md` ("Data")
- Test: `plugins/last-curtain/tests/NightDataTests.cpp`

**Interfaces:**
- Produces:
  - `enum class Move : std::uint8_t { Heavy, Light, Taunt };`
  - `struct PropCue { int item = -1; Move cue = Move::Light; };`
  - `struct ScriptDef { std::string id, name, text; int act = 1, specter = -1; std::vector<PropCue> props; int base = 0, perDamage = 0, perProp = 0, onCue = 0; };`
  - `struct SpecterDef { std::string id, name, text; int hp = 0, defense = 0, heavy = 0, light = 0, taunt = 0; std::vector<Move> moves; };`
  - `Rules::acts { std::vector<int> rounds; int visit = 1, event = 2, monster = 3, killShare = 50; }`, `Rules::duel { int exchanges = 3; }`, `Rules::ghost` gains `int reviveHpPercent = 50;` (drop `respawnRounds`).
  - `ItemDef::prop` (bool), `ItemDef::copies` (int, 0 for non-props).
  - `NightDef::scripts`, `NightDef::specters`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("The programme loads: four acts, a script for each, its specter and props (T274)", "[night][data]")
{
    const NightDef def = LoadTestNight();
    CHECK(def.rules.acts.rounds == std::vector<int>{3, 6, 9, 12});
    CHECK(def.rules.duel.exchanges == 3);
    for (int act = 1; act <= 4; ++act)
        CHECK(std::any_of(def.scripts.begin(), def.scripts.end(), [&](const ScriptDef &s) { return s.act == act; }));
    for (const ScriptDef &s : def.scripts)
    {
        CHECK(s.specter >= 0);
        CHECK(s.props.size() == 2);
        for (const PropCue &p : s.props)
            CHECK(def.items[U(p.item)].prop);
    }
    for (const SpecterDef &s : def.specters)
        CHECK_FALSE(s.moves.empty());
}

TEST_CASE("Bad programme data is refused with its file and field (T275)", "[night][data]")
{
    // Each case copies the test data folder, breaks one value, and expects a diagnostic naming it.
    CHECK(Refused("scripts.csv", "specter", "nobody").field == "scripts.csv:2:specter");
    CHECK(Refused("scripts.csv", "props", "bandage:light").message.find("prop") != std::string::npos); // not a prop item
    CHECK(Refused("scripts.csv", "props", "mask:loud").field == "scripts.csv:2:props");         // an unknown cue
    CHECK(Refused("specters.csv", "moves", "").field == "specters.csv:2:moves");
    CHECK(Refused("rules.json", "acts.rounds", "[3, 2]").code == "night.rules");                // not rising
    CHECK(Refused("rules.json", "acts.rounds", "[3, 13]").code == "night.rules");               // past the clock
}
```

`Refused(file, field, value)` is a test helper next to the existing data-test helpers: it copies
`assets/night` to a temp folder, sets the first data row's `field` (or the JSON path) to `value`,
loads, and returns the first diagnostic.

- [ ] **Step 2: Run them to see them fail**

Run: `cmake --build /home/claude/build-gcc --target UmbraNightTests && /home/claude/build-gcc/plugins/last-curtain/tests/UmbraNightTests "[data]"`
Expected: FAIL to compile (`scripts`, `ScriptDef` unknown).

- [ ] **Step 3: Add the types and loading**

Columns: `scripts.csv` = `id,name,act,specter,props,text,base,perDamage,perProp,onCue` (props as
`item:cue;item:cue`, cue one of `heavy`, `light`, `taunt`); `specters.csv` =
`id,name,hp,defense,heavy,light,taunt,moves,text` (moves as `heavy;light;taunt` with repeats as
weights); `items.csv` gains `prop` (0/1) and `copies` at the end. Checks: act 1..`acts.rounds.size()`,
specter and items exist and are props, a prop's `copies` is 1 or 2 and others 0, `acts.rounds`
rising and within `clock.rounds`, every act has a script.

Data (my content, to tune in the soak): props = lantern, keys (act I); sword, mask (II); fan, ledger
(III); crown, watch (IV), each `copies` 2 except crown and watch 1. Four specters (`prompter`,
`weaver`, `understudy`, `diva`) with hp 8/10/12/14, defense 12/13/13/14, heavy 4/5/5/6, light 2/2/3/3,
taunt 2/3/3/4, moves `heavy;light;light;taunt` (prompter), `heavy;heavy;light;taunt` (weaver),
`light;taunt;taunt;heavy` (understudy), `heavy;light;taunt` (diva). One script per act: base 6/8/10/14,
perDamage 1, perProp 2/3/3/4, onCue 2/3/3/4, cues lantern:taunt, keys:light, sword:light, mask:taunt,
fan:heavy, ledger:taunt, crown:heavy, watch:light. `rules.json`: `"acts": { "rounds": [3,6,9,12],
"visit": 1, "event": 2, "monster": 3, "killShare": 50 }`, `"duel": { "exchanges": 3 }`, ghost
`"reviveHpPercent": 50` (and `respawnRounds` removed).

- [ ] **Step 4: Run the tests; update the fingerprint**

Run: as Step 2. Expected: T274, T275 PASS; the fingerprint tests fail with the new value; update
both golden values and rerun the whole suite. Expected: all pass.

- [ ] **Step 5: Commit** — `issue #UMBRA-247 The programme's data: acts, scripts, specters, props`

---

### Task 2: Applause, the kill share, and the winner at dawn

**Files:**
- Modify: `modules/night/include/Night.hpp` (`Guest::applause`, `Guest::seen`; `Outcome { Running, Finale }`; `LogKind::Applause`)
- Modify: `modules/night/src/Turn.cpp` (visit, event; `Hurt` kill share and no last-alive ending), `Battle.cpp` (monster beaten), `Night.cpp` (`Dawn` ends the night)
- Create: `modules/night/src/Acts.cpp` (`AddApplause`, `Winner`)
- Modify: `apps/LastCurtain/Play.cpp:1225,1511-1514` and `modules/nightbots/src/NightSoak.cpp:63` (callers of the old outcomes; the app shows "the curtain falls; <winner> takes the stage" with the existing ending scene `ending_escaped`)
- Test: `tests/ActsTests.cpp` (new, added to `tests/CMakeLists.txt`); retire the last-alive and house-win cases in `tests/RoyaleTests.cpp` (ledger each)

**Interfaces:**
- Produces: `void inner::AddApplause(NightState &state, int guest, int amount, int reason);` (logs `LogKind::Applause`, who, a = amount, b = reason: 0 visit, 1 event, 2 monster, 3 kill, 4 duel); `int Winner(const NightState &state);` (public, in `Night.hpp`).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("Applause: a first visit, a passed event, a beaten monster (T276)", "[night][acts]")
TEST_CASE("Killing a guest takes half their Applause; the house takes none (T277)", "[night][acts]")
// victim 9 -> killer +4, victim 5; a trap death: victim keeps 9
TEST_CASE("Nobody wins by being last alive; dawn ends the night on Applause (T278)", "[night][acts]")
// one living guest at round 5: outcome Running; after round 12 (no acts in this test's rules): outcome Finale, winner = most Applause
TEST_CASE("A tie at dawn goes to the living, then the level, then the turn order (T279)", "[night][acts]")
```

Each sets up `NewNight` with the test data, sets fields directly, and drives `Apply` or `Hurt`.

- [ ] **Step 2: Run them to see them fail** — `UmbraNightTests "[acts]"`; Expected: compile FAIL.
- [ ] **Step 3: Implement** — `Guest::applause` (`std::int16_t`), `Guest::seen` (`std::bitset<maxCells>`); `Hurt` no longer ends the night; `Dawn` calls `EndNight(state, Outcome::Finale)` with `state.winner = Winner(state)`.
- [ ] **Step 4: Run the whole suite** — Expected: all pass (retired Royale cases noted in the ledger).
- [ ] **Step 5: Commit** — `issue #UMBRA-248 Applause on the board, the kill share, the winner at dawn`

---

### Task 3: Ghosts stay, and come back

**Files:**
- Modify: `modules/night/src/Turn.cpp` (`Hurt`: a ghost where it fell; ghost legal actions; relic use), `Battle.cpp` (`RoundStart`: no rising at the under-stage), `Company.cpp` (no attacks on ghosts)
- Modify: `modules/night/include/Night.md` ("Ghosts and the killer")
- Test: `tests/ActsTests.cpp`

**Interfaces:**
- Produces: `void inner::Revive(const NightDef &def, NightState &state, int guest);` (living again at `ceil(maxHp * reviveHpPercent / 100)`, logs `LogKind::Rose`).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("A guest killed on the board is a ghost where it fell, its items on the floor (T280)", "[night][acts]")
TEST_CASE("A ghost cannot attack and cannot be attacked (T281)", "[night][acts]")
// no Attack in a ghost's LegalActions; no Attack with b = the ghost in a living guest's
TEST_CASE("A ghost lives again in the Dressing room at half HP (T282)", "[night][acts]")
// maxHp 13 -> hp 7
TEST_CASE("A relic revives a ghost: picked up by the ghost, or used on it by the living (T283)", "[night][acts]")
```

- [ ] **Step 2: Run them to see them fail** — Expected: FAIL (ghosts vanish to cell -1 today).
- [ ] **Step 3: Implement** — `Hurt` keeps `cell`; ghosts' legal actions: Move, PickUp (relic only), UseItem (relic on self), EndTurn; entering `dressing` revives; a living guest's relic targets a ghost in its room.
- [ ] **Step 4: Run the whole suite** — Expected: all pass.
- [ ] **Step 5: Commit** — `issue #UMBRA-249 Ghosts stay where they fell and revive at the Dressing room or by a relic`

---

### Task 4: The act's props on the floor

**Files:**
- Modify: `modules/night/include/Night.hpp` (`NightState::act` (int8, 0 = before act I), `scripts` (`std::array<std::int8_t, 8>`: the script row drawn for each act))
- Modify: `modules/night/src/Acts.cpp` (`DrawScripts`, `AnnounceAct`), `Night.cpp` (`NewNight` draws and announces act I), `Turn.cpp` (`DrawItem` skips props), `Company.cpp` (`PlaceKeepsake` skips props)
- Test: `tests/ActsTests.cpp`

**Interfaces:**
- Produces: `void inner::AnnounceAct(const NightDef &def, NightState &state, int act);` (lays the act's props); `const ScriptDef *ActScript(const NightDef &def, const NightState &state, int act);` (public; null past the last act).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("A new night lays act I's props, and only those (T284)", "[night][acts]")
// floor holds lantern x2 and keys x2, each in an opened room other than foyer and dressing, no guest there
TEST_CASE("Props never come from item rooms or goals (T285)", "[night][acts]")
// 200 seeds: resolve every item room; no prop is ever given
TEST_CASE("A prop already carried is not laid again for its act (T286)", "[night][acts]")
```

- [ ] **Step 2: Run them to see them fail.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run the whole suite.**
- [ ] **Step 5: Commit** — `issue #UMBRA-250 Each act's props are laid when it is announced`

---

### Task 5: The act and the duel

**Files:**
- Modify: `modules/night/include/Night.hpp` (`struct Duel { bool active; std::int8_t guest, exchange, move; std::int16_t specterHp, damage; std::uint8_t used, cues; std::array<std::int8_t, maxGuests> queue; std::int8_t next; };` in `NightState`; `ActionKind::DuelStrike, DuelHold, DuelProp`; `LogKind::Curtain` (a = act), `Exchange` (who, a = action, b = damage taken), `Performed` (who, a = Applause, b = 1 booed))
- Modify: `modules/night/src/Acts.cpp` (`StartAct`, `DuelLegal`, `DuelApply`, `EndDuel`), `Turn.cpp` (`EndTurn` starts the act at its round; `Current`, `LegalActions`, `Apply` route to the duel)
- Modify: `modules/night/include/Night.md` (new section "Acts and the duel")
- Test: `tests/ActsTests.cpp`

**Interfaces:**
- Consumes: `AddApplause` (Task 2), `AnnounceAct`, `ActScript` (Task 4).
- Produces: `bool InDuel(const NightState &state);`, `Move ShownMove(const NightState &state);` (public, for bots and screens).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("After round 3's last turn every living guest performs in turn order; ghosts do not (T287)", "[night][acts]")
TEST_CASE("Strike, Hold and a prop answer the shown move as the rules say (T288)", "[night][acts]")
// set the shown move and rolls through a fixed seed; check damage dealt and taken for each of the 9 pairs
TEST_CASE("A duel's Applause: base standing + damage + props + cues; booed off keeps all but base (T289)", "[night][acts]")
TEST_CASE("The finale at round 12 is followed by dawn, and the next act is announced after each (T290)", "[night][acts]")
TEST_CASE("Every guest a ghost at curtain: the act passes and the night goes on (T291)", "[night][acts]")
```

- [ ] **Step 2: Run them to see them fail.**
- [ ] **Step 3: Implement** — the move is drawn (state RNG, uniform over `moves`) at each exchange's start and stored, so a save between exchanges keeps it.
- [ ] **Step 4: Run the whole suite.**
- [ ] **Step 5: Commit** — `issue #UMBRA-251 The act: each living guest duels the specter alone`

---

### Task 6: Quests go

**Files:**
- Delete: `assets/night/quests.csv`, `modules/night/src/Quests.cpp`
- Modify: `NightDef.hpp`, `NightData.cpp`, `Night.hpp` (`quest`, `questBy`, `LogKind::Quest`, `QuestDef`, `maxQuests`), `Turn.cpp`, `Battle.cpp`; the app's quest panel and book (`Play.cpp`, `Play.hpp`, `assets/ui/book.ui.json`, the HUD Quests button) and `Narrate.cpp` quest captions; `NightSoak` quest counters; `Night.md`, `Board.md`, `Play.md`
- Test: retire quest tests (`TurnTests`, `PlayTests` T265/T266, `NarrateTests`), each in the ledger

- [ ] **Step 1: Remove; build.** Run the build. Expected: compiles with no `Quest` reference left (`grep -rn "Quest" plugins/last-curtain --include=*.cpp --include=*.hpp` shows none).
- [ ] **Step 2: Run the whole suite; update the fingerprint.** Expected: all pass.
- [ ] **Step 3: Commit** — `issue #UMBRA-252 Quests are removed: the acts' scripts replace them`

---

### Task 7: Bots perform, and the soak reports Applause

**Files:**
- Modify: `modules/nightbots/src/NightBots.cpp` (duel choice; props first when picking up), `NightSoak.cpp`, `include/NightSoak.hpp` (report fields), `include/Balance.md`
- Modify: `assets/night/rules.json` `balance` (`winnerMax` 35 stays; add `natureMinPercent` 8, `leadActsMax` 1)
- Test: `tests/BotTests.cpp`

**Interfaces:**
- Produces: `SoakReport` gains `std::array<int, maxGuests> wins` (by Applause), `long long props, cues, duels, booed`, `long long leadTotal` (winner minus second, summed), `std::array<int, 3> natureWins`; `Holds` checks no faults, deaths per night, `winnerMax`, `natureMinPercent`, and the mean lead under `leadActsMax` x the mean Applause of an act.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("A bot answers the shown move: a prop on its cue, Hold a heavy blow, else Strike (T292)", "[night][bots]")
TEST_CASE("A bot picks up the next act's prop before anything else on the floor (T293)", "[night][bots]")
TEST_CASE("100 bot nights end at dawn with a winner by Applause (T294)", "[night][bots]")
```

- [ ] **Step 2: Run them to see them fail.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run the suite and a soak** — `UmbraTool night-soak --runs 500` (or the existing soak command); record the report in `docs/testing/s40-performances.md` (new, from the ROADMAP section 14 template). Expected: no faults; the balance bands may fail and are reported, not tuned (tuning is plan 2's).
- [ ] **Step 5: Commit** — `issue #UMBRA-253 Bots perform duels; the soak reports Applause`

---

### Task 8: Saves version 7

**Files:**
- Modify: `modules/night/src/NightSave.cpp` (version 7; `Guest` applause, seen; `NightState` act, scripts, duel)
- Test: `tests/SaveTests.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("A save in the middle of a duel loads to the same performer, exchange and shown move (T295)", "[night][save]")
TEST_CASE("A version 6 save is refused with a reason (T296)", "[night][save]")
```

- [ ] **Step 2: Run them to see them fail.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run the whole suite.**
- [ ] **Step 5: Commit** — `issue #UMBRA-254 Saves keep Applause, the act and the duel (version 7)`

---

### Task 9: The app: a duel you can play, and the ending

**Files:**
- Modify: `apps/LastCurtain/Play.cpp`, `Play.hpp` (a `duel` menu group: Strike, Hold, one entry per usable prop, with the shown move in the menu's title), `modules/board/src/Narrate.cpp` and `assets/night/captions.json` (captions: curtain, exchange, performed, applause, revived, finale)
- Modify: `apps/LastCurtain/Play.md`
- Test: `tests/PlayTests.cpp`, `tests/NarrateTests.cpp`

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("In a duel the menu offers Strike, Hold and the carried script props, and names the shown move (T297)", "[night][play]")
TEST_CASE("Curtain, exchange, performance and the finale are told (T298)", "[night][narrate]")
```

- [ ] **Step 2: Run them to see them fail.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run the whole suite, `verify.sh`, and a capture** — `bash /home/claude/verify.sh`; Expected: GCC, Clang and ASan all pass. A SwiftShader capture of a duel menu (`LastCurtain --seed 5 --watch`), looked at.
- [ ] **Step 5: Commit** — `issue #UMBRA-255 Duels can be played from the menu; the finale names the winner`
