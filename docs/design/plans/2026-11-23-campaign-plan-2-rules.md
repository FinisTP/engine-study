# Campaign plan 2: the fight rules — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the faction match's fight engine (`match` module) serve the campaign too: rule switches set by data, one Speed order for every unit, falls that stay down, revive and removal, biding and action points, the gold knot and freeing, the boss win, game over and Lệ Thanh's rite. The faction match keeps playing as before, its tests and balance soak passing.

**Architecture:** A `Mode` value (the spec's "Rules"; that name is taken by `MatchDef::Rules`, the number tables) lives in `MatchState` and is copied from `rules.json` `"modes"` at the start; every new rule asks it. The monsters' phase is removed: `order` holds every unit and `NextTurn` runs a monster's behaviour at its own place. New actions (`Bide`, `Extra`, `Cut`, `Rite`) and ability classes (`revive`, `remove`, `giveap`) go through the same `Check`/`Apply`. Headless only; the screen work is plan 7, stage files plan 4, the save of the rite plan 5.

**Tech Stack:** C++20, Catch2, nlohmann/json; `plugins/last-curtain/modules/match`, `modules/matchbots`, the app's `MatchPlay`.

**Spec:** `docs/design/2026-11-23-campaign-slice-1.md` §7.1–7.3 (and the Project's handoff `campaign-slice-1-handoff-2026-11-23.md` §2.1–2.6, §9.2). Read `modules/match/include/Match.md` first.

## Global Constraints

- Code style (CLAUDE.md): short, simple C++; `// 1.` step comments only where needed; rules explained in `Match.md`; `umbra::match`; `PascalCase`; Catch2 in `plugins/last-curtain/tests/`. C++ string literals ASCII.
- All numbers are data (`rules.json`, CSVs). Rules stay headless. Player input, bots and monsters change the state only through `Apply` (monsters through the rules' own turn, as now).
- **Faction match unchanged except the turn order**: its switches are `essence`, `perform`, `respawn`, `fightXp` on; `biding`, `fallsStay`, `playerLoss` off; `win` score; `roundLimit` = `options.csv` rounds (30). Its tests and `[match-balance]` soak must pass with the bands in `rules.json` `"balance"` **unchanged** (side 45–55%, comebacks ≥ 25%…). If the one Speed order moves the soak out of its bands, retune data (monster speeds, spawn rows), never the bands, and record it in `Bots.md`'s tuning record.
- **Campaign switches**: `essence`, `perform`, `respawn`, `fightXp` off; `biding`, `fallsStay`, `playerLoss` on; `win` boss; `roundLimit` 0 (none); `maxAp` 2; `apPerTurn` 2.
- Biding (spec 7.2): a bide is the whole turn (no move, no action) and gives **1 AP**; a unit stores **at most 2**; spends **at most 2 a turn**; each AP spent buys one more move and action; an ability's AP is paid first from the same pool and limit; no Defence bonus; **AP is 0 at the start and lost on a fall**.
- Falls (7.3): a fallen unit stays on its square, can't be hit, is skipped in the order, doesn't block and doesn't count toward `squareCapacity`.
- A unit that appears mid-fight or is revived joins the order at its Speed and acts **from the next round**. Ties keep today's seeded key.
- Only the unit flagged as the living player cuts strings. Stage 1's extra condition ("Tí freed") is the general `mustFree`: if the boss falls while a recruit is still strung (or a recruit is removed), the fight is lost.
- The rite revives the whole player side to full HP, once in the whole game; offered only after a loss by the player falling; the fight goes on.
- Commits `issue #UMBRA-<n> <summary>` from **#334**, ending with the session's `Co-Authored-By` and `Claude-Session` lines.

## Review Focus

1. **A battle in progress when someone falls**: today `Down` sets `cell = -1` and the battle ends when a group has nobody standing. With falls, the fallen keep their square; a battle must still end when a group has no one *standing* (hp > 0), and nobody may pick for or strike a fallen fighter. Test in Task 4: a 1-vs-1 battle where the defender falls ends the battle with the defender fallen on its square.
2. **The order with nobody able to act**: a round where every unit is fallen, removed or waiting to join must not loop forever (today's guard `order.size() * 4`). Test in Task 2: a state where only fallen units and a just-revived one remain advances to the next round and the revived unit acts.
3. **Revive onto a full square**: a square holding 2 standing allies (and the fallen one) puts the revived unit on the nearest free square, ties nearest the reviver, then the lower cell index. Test in Task 4.
4. **The player falls during an enemy's battle** (not on the player's own turn): the loss happens at once, mid-battle; the rite then ends that battle and the order goes on from the next unit. Test in Task 6.
5. **Old saves**: version 3 saves (faction match) are refused with the version message, not misread. Test in Task 1.

---

## File structure

| File | Change |
|---|---|
| `modules/match/include/MatchDef.hpp` | `Mode`, `WinBy`; `MatchDef::modes`; `AbilityDef::ap`; ability classes `Revive`, `Remove`, `GiveAp`; item class `Revive` |
| `modules/match/src/MatchData.cpp` | read `rules.json` `"modes"`; `abilities.csv` `ap` column, new classes; `items.csv` class `revive` |
| `modules/match/include/Match.hpp` | `Unit` flags, `ap`, `from`; `Up`, `Fallen`; `TurnState::apSpent`; `MatchState::mode`, `loss`, `riteReady`, `riteUsed`; `Loss`; actions `Bide`, `Extra`, `Cut`, `Rite`; new `Why`s; `FromCharacter`, `FromMonster`, `BeginMatch` public; `monsterTurn` and `RunMonsters` removed |
| `modules/match/src/Turn.cpp` | the one order, joining, switches in turn flow, `Bide`, `Extra`, `CheckEnd` |
| `modules/match/src/Monsters.cpp` | `Act` called from the order; `MonsterPhase` removed |
| `modules/match/src/Battle.cpp` | `Down` → fall or today's down by mode; battle ends on no one standing |
| `modules/match/src/Abilities.cpp` | AP cost, `revive`, `remove`, `giveap`, revive item |
| `modules/match/src/Fight.cpp` (new) | `Cut`, `Rite`, `CheckEnd`, `RevivePlace` (the campaign's end and freeing) |
| `modules/match/src/Match.cpp`, `Stage.cpp`, `MatchSave.cpp` | `BeginMatch`; perform switch; save version 4 |
| `modules/match/include/Match.md` | sections "Modes", "The order", "Falls", "Biding", "Freeing and the end" |
| `assets/match/rules.json`, `abilities.csv` | `"modes"`; `ap` column (0 everywhere) |
| `apps/LastCurtain/MatchPlay.cpp`, `assets/match-words.json` | order strip without `monsterTurn`; texts for the new `Why`s |
| `tests/MatchTestData.hpp`, `tests/MatchFightTests.cpp` (new), `tests/MatchMonsterTests.cpp`, `tests/MatchTurnTests.cpp`, `tests/CMakeLists.txt` | the campaign fixture and the new tests |

---

### Task 1: Modes as data, the new unit and state fields, save version 4 (#334)

**Files:** `MatchDef.hpp`, `MatchData.cpp`, `Match.hpp`, `Match.cpp`, `MatchSave.cpp`, `rules.json`, `Match.md` ("Modes"), `tests/MatchDataTests.cpp`, `tests/MatchSaveTests.cpp`, `tests/MatchTestData.hpp`.

**Interfaces — Produces:**

```cpp
// MatchDef.hpp
enum class WinBy : std::uint8_t { Score, Boss };
struct Mode
{
    bool essence = true, perform = true, respawn = true, fightXp = true; // the faction match's
    bool biding = false, fallsStay = false, playerLoss = false, mustFree = false;
    WinBy win = WinBy::Score;
    int roundLimit = 0; // 0 = none; the faction mode takes options.csv rounds when rules.json leaves it out
    int maxAp = 2, apPerTurn = 2;
};
// MatchDef: std::map<std::string, Mode> modes; // rules.json "modes": "faction" and "campaign" required

// Match.hpp
namespace flag { constexpr std::uint8_t Player = 1, Boss = 2, Recruit = 4, Strung = 8, Freed = 16, Removed = 32; }
// Unit: std::uint8_t flags = 0; std::int8_t ap = 0; std::int16_t from = 0; // first round it may act
inline bool Up(const Unit &u) { return u.cell >= 0 && u.hp > 0; }      // on the board and standing
inline bool Fallen(const Unit &u) { return u.cell >= 0 && u.hp <= 0; } // only with mode.fallsStay
enum class Loss : std::uint8_t { None, PlayerFell, SideDown, Trapped };
// TurnState: std::int8_t apSpent = 0;
// MatchState: Mode mode; Loss loss = Loss::None; bool riteReady = false, riteUsed = false;
Unit FromCharacter(const MatchDef &def, int row, int faction, int home); // moved out of the anonymous namespace
Unit FromMonster(const MatchDef &def, int row, int faction, int cell);   // moved out of inner
```

`rules.json` gains:

```json
"modes": {
  "faction":  { "essence": true,  "perform": true,  "respawn": true,  "fightXp": true,  "biding": false, "fallsStay": false, "playerLoss": false, "mustFree": false, "win": "score" },
  "campaign": { "essence": false, "perform": false, "respawn": false, "fightXp": false, "biding": true,  "fallsStay": true,  "playerLoss": true,  "mustFree": false, "win": "boss", "roundLimit": 0, "maxAp": 2, "apPerTurn": 2 }
}
```

(`mustFree` is a stage's extra condition: plan 4's Stage 1 file turns it on.) `NewMatch` sets `s.mode = def.modes.at("faction")` with `roundLimit = def.options.rounds` when the file leaves it out. **Audit**: every rules check that means "a unit that can act, be hit, block or be counted" and reads `cell >= 0` becomes `Up(u)` (Turn, Battle, Abilities, Monsters, MonsterMind, Stage, Treasure, `matchbots`); in the faction mode both are the same, so no behaviour changes.

- [ ] **Step 1: Failing tests.** In `MatchDataTests.cpp`: `"rules.json modes load, and a broken mode is refused"` — `def.modes.at("campaign").fallsStay`, `!def.modes.at("faction").biding`, `def.modes.at("campaign").win == WinBy::Boss`, `maxAp == 2`; a copy with `"win": "most"` → the first diagnostic's field names `modes.campaign.win`; a copy without `"campaign"` → refused. In `MatchSaveTests.cpp`: extend T308 by setting `s.units[0].flags = flag::Player`, `ap = 2`, `from = 3`, `s.mode = def.modes.at("campaign")`, `s.riteUsed = true`, `s.loss = Loss::Trapped` before saving; round trip equal (`Fingerprint` and those fields). T309: a save with `"version": 3` → refused, error names the version. In a new case: `NewMatch(...).mode.roundLimit == def.options.rounds`.
- [ ] **Step 2:** Build `UmbraMatchTests`; run `[match][data],[match][save]` — FAIL (no `modes`).
- [ ] **Step 3: Implement** the types, the loader (`"win"`: `score` or `boss`; unknown key in a mode → refused with its field), `FromCharacter`/`FromMonster` public, the `Up` audit, save macros with the new fields and `version = 4` (comment: "4: modes, falls, AP, flags").
- [ ] **Step 4:** Run all of `Umbra.Match` (not the soak) — PASS, nothing else changed.
- [ ] **Step 5: Commit** `issue #UMBRA-334 Match: modes as data (the faction match's and the campaign's switches), unit flags, AP and join round, save version 4`.

---

### Task 2: One Speed order for every unit (#335)

**Files:** `Match.hpp`, `Match.cpp`, `Turn.cpp`, `Monsters.cpp`, `MatchInner.hpp`, `Match.md` ("A turn", "Monsters", new "The order"), `apps/LastCurtain/MatchPlay.cpp`, `tests/MatchMonsterTests.cpp`, `tests/MatchTurnTests.cpp`, `tests/MatchTestData.hpp`.

**Interfaces:**
- Produces: `void BeginMatch(const MatchDef &def, MatchState &s, std::uint64_t seed);` — sorts `order` over **all** units (Speed high first, ties by a key drawn from `seed`, as today), sets `round = 1`, `turn = -1` and starts the first turn through `NextTurn`. `NewMatch` ends with it; plan 4's stage builder will call it. `MatchState::monsterTurn` and `RunMonsters` are removed; `MonsterPhase` is gone.
- Rule (`NextTurn`): advance `turn`; past the end → `NewRound` (spawns, room spawns, then from the top). The unit at `order[turn]` is skipped unless `Up(u)` and `u.from <= s.round` (a down faction character still counts its `back` at its own place, as today). A monster acts there by its behaviour (`Act`), then the order goes on, unless `Act` started a battle: then `Current` is the picker and, when that battle ends, `Apply` continues with `NextTurn` (replacing today's `ResumeMonsters`). A unit placed by a spawn, a revive or a return gets `from = s.round + 1`.
- Test fixture (`MatchTestData.hpp`): `MatchState Fight(const MatchDef &def, std::uint64_t seed)` — `BuildBoard`, `mode = campaign`, units: player (`FromCharacter` of the All-Rounder row, faction 0, `flag::Player`), an ally (Supporter row, faction 0), a boss (Attacker row, faction 1, `flag::Boss`), a recruit (Speedster row, faction 1, `flag::Recruit | flag::Strung`), placed on 4 hall squares in a row near the board's middle, then `BeginMatch`. Plus `void Put(MatchState &s, int unit, int cell)`.

- [ ] **Step 1: Failing tests** (`MatchTurnTests.cpp`, `MatchMonsterTests.cpp`):
  - `"Every unit, monsters included, takes its turn by Speed"`: in a faction match, `order` holds every unit; a monster with higher Speed than a character comes before it; ties are the same for the same seed.
  - `"A monster acts at its own place in the order"`: a `stage` monster with Speed between two characters moves when the first character ends its turn, before the second acts.
  - `"A unit that appears mid-round acts from the next round"`: a spawn placed in round 2 with the highest Speed does not act in round 2, does in round 3.
  - `"A hunter's battle in the order waits for the picks, then the order goes on"` (rewrite T318): after the battle the next unit in the order acts.
  - `"A round where no one can act still moves on"` (Review Focus 2): only fallen units and one revived (`from` = next round) → `NextTurn` reaches the next round and the revived unit is `Current`.
  - Update T299 to the new flow (no `RunMonsters`).
- [ ] **Step 2:** Run `[match]` — the new ones FAIL.
- [ ] **Step 3: Implement** `BeginMatch`, the loop above, `Act` reached from `NextTurn`, removal of the phase; `MatchPlay` order strip: the strip lists `order` from the current place, skipping units that are not `Up` (monsters included, as spec §8 wants); "their turn" shows for a monster's place too.
- [ ] **Step 4:** Run `Umbra.Match` — PASS. Then the soak: `UmbraMatchTests "[match-balance]"`. If it fails, retune data (not bands) until it passes, run all `[match]` again, and write what changed and the soak's numbers in `modules/matchbots/include/Bots.md`'s tuning record.
- [ ] **Step 5:** Build `LastCurtain`; `LastCurtain --watch --seed 5 --frames 90 --capture` under SwiftShader: a match still plays (monsters move between characters' turns).
- [ ] **Step 6: Commit** `issue #UMBRA-335 Match: one Speed order for every unit, monsters included; units that appear act from the next round; the monsters' phase removed`.

---

### Task 3: The switches in play (#336)

**Files:** `Turn.cpp`, `Battle.cpp`, `Abilities.cpp`, `Stage.cpp`, `Monsters.cpp`, `Match.hpp` (`Why::Off`), `Match.md` ("Modes"), `assets/match-words.json` (`why.Off`), `tests/MatchFightTests.cpp` (new; add to `UmbraMatchTests`).

**Interfaces:** `Why::Off` ("not in this fight"). Each switch, off: `essence` — `Down` moves or drops no essence, floors get none, `score` never changes; `perform` — `Check(Perform)` is `Off`; `respawn` — no `back` count, no den respawn (`ScheduleSpawn`), no hub healing in `Commit`; `fightXp` — no `GainXp` from downs, abilities or items, so no level choice mid-fight; `roundLimit` 0 — `NewRound` never ends the fight on rounds, `TurnsLeft` returns -1.

- [ ] **Step 1: Failing tests** `"Each switch does its part, and the faction match keeps them all"` — one `SECTION` per switch, run on `Fight()` (campaign) and on `NewMatch` (faction): e.g. a boss downed by the player in the campaign: player `essence` 0, `kills` unchanged XP 0, no floor entry; the same down in the faction match: essence moved, XP gained. `Check(def, s, {ActionKind::Perform})` is `Off` in the campaign. Campaign round 31 does not end the fight; faction round 31 does.
- [ ] **Step 2:** Run `[match][fight]` — FAIL.
- [ ] **Step 3: Implement** the guards (each a one-line `if (s.mode.x)` where the effect happens).
- [ ] **Step 4:** `Umbra.Match` — PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-336 Match: the switches in play (essence, perform, respawn, fight XP, round limit)`.

---

### Task 4: Falls, reviving and removal (#337)

**Files:** `Battle.cpp` (`Down`), `Turn.cpp` (`UnitAt`, `Room`), `Abilities.cpp`, `Fight.cpp` (new, `RevivePlace`), `MatchDef.hpp`, `MatchData.cpp`, `Match.hpp`, `Match.md` ("Falls"), `match-words.json`, `tests/MatchFightTests.cpp`.

**Interfaces:**
- Ability classes `revive` (board, target `ally`; `power` = percent of max HP it stands up with, at least 1 HP) and `remove` (board, target `enemy`); item class `revive` (`amount` = percent). `Why::Fallen` (the target has fallen), `Why::NotFallen` (revive/remove need a fallen target).
- `Down` with `fallsStay`: `hp = 0`, `ap = 0`, **cell kept**, logged `Down`; the battle ends when a group has no fighter `Up`. Without it: today's behaviour.
- `UnitAt`, `Room` and the battle's targeting ignore fallen units; every hit, strike, heal, give-AP and attack refuses a fallen target with `Fallen`.
- `int RevivePlace(const MatchDef &def, const MatchState &s, int unit, int reviver);` — its own square if `Room` allows, else the nearest square by `Steps` where `Room` allows, ties by `Steps` from the reviver, then the lower cell index; -1 if none (the revive is refused, `Blocked`).
- Revive: `hp = max(1, maxHp * percent / 100)`, placed by `RevivePlace`, `from = s.round + 1`, logged `Healed`. Remove: `cell = -1`, `flag::Removed`, logged `Down` with c = -1; a removed unit can't be revived (`NotFallen`).
- Enemies revive their own through the same actions (no rule says only the player side may).

- [ ] **Step 1: Failing tests** in `MatchFightTests.cpp` (add the two abilities and the item to a `MatchDef` copy in the test: `revive` power 30, `remove`, item `revive` amount 50):
  - `"A fallen unit stays on its square, can't be hit, is skipped and doesn't block"`: the boss falls in battle → battle over (Review Focus 1), boss cell unchanged, `Attack` on it → `Fallen`; the player walks onto its square; two allies may stand there (capacity 2 not counting it); the boss's place in the order is skipped.
  - `"AP is lost on a fall"`: ally with `ap = 2` falls → 0.
  - `"Revive stands a fallen ally up with part of its HP, on the nearest free square when its own is full"` (Review Focus 3): ally maxHp 20, revive 30% → hp 6; its square holding two standing allies → nearest free square, the tie-break by the reviver's distance, then by cell index; acts from the next round.
  - `"A revive item works like the ability"`; `"Revive and remove need a fallen target"`; `"A removed unit is gone for the fight and can't be revived"`; `"Enemies revive their own"`.
- [ ] **Step 2:** Run — FAIL.
- [ ] **Step 3: Implement**; loader: classes `revive`, `remove` on the board only, targets as above; item class `revive`.
- [ ] **Step 4:** `Umbra.Match` — PASS (faction unchanged: it never falls).
- [ ] **Step 5: Commit** `issue #UMBRA-337 Match: falls that stay down, revive (ability and item, nearest free square), removal`.

---

### Task 5: Biding and action points (#338)

**Files:** `Turn.cpp`, `Abilities.cpp`, `MatchDef.hpp`, `MatchData.cpp`, `abilities.csv` (`ap` column, 0 everywhere), `Match.hpp`, `Match.md` ("Biding"), `match-words.json`, `tests/MatchFightTests.cpp`.

**Interfaces:**
- `ActionKind::Bide` — `Check`: `Off` unless `mode.biding`; only before moving or acting this turn (`cell == now.origin`, `movesLeft == move`, `!acted`, `apSpent == 0`), else `AlreadyActed`. `Apply`: `ap = min(ap + 1, maxAp)`, log, `NextTurn`.
- `ActionKind::Extra` — spend 1 AP after the turn's action: `Check` needs `acted`, `ap >= 1`, `apSpent < apPerTurn` (`NoAp` otherwise). `Apply`: `ap -= 1`, `apSpent += 1`, `acted = false`, `movesLeft = move`, `origin = cell`.
- `AbilityDef::ap` (column `ap`, board abilities only; the loader refuses `ap > 0` on a battle or passive ability): `CheckBoardAbility` adds `NoAp` when `ap < cost` or `apSpent + cost > apPerTurn`; `BoardAbility` pays it first. `Why::NoAp` text: "not enough action points".
- Class `giveap` (board, target `ally`, range as data): moves 1 AP from the user to the target; refused (`NoAp`) when the user has none or the target holds `maxAp`.
- Monsters don't bide in this plan (their behaviours come with plan 4).

- [ ] **Step 1: Failing tests:** `"Biding is the whole turn and gives 1 AP, at most 2"` (three bides → 2); `"A bide is refused after a move, after an action, and in the faction match (Off)"`; `"With 2 AP a turn has three actions, each with its own move"` (act, `Extra`, move `move` steps, act, `Extra`, act; a third `Extra` → `NoAp`); `"An AP ability is paid first from the same pool and limit"` (2 AP: a 1-AP ability then one `Extra` and no more; a 2-AP ability then no `Extra`; 0 AP → the ability is `NoAp`, and `Legal` leaves it out); `"Give-AP moves one AP to an ally"`; `"Every unit starts a fight at 0 AP"` (`BeginMatch`).
- [ ] **Step 2:** Run — FAIL.
- [ ] **Step 3: Implement**; `abilities.csv` gets the `ap` column with 0 in every row.
- [ ] **Step 4:** `Umbra.Match` — PASS; the faction `Legal` never offers `Bide` or `Extra` (a test line in T303's bot run: no such action chosen).
- [ ] **Step 5: Commit** `issue #UMBRA-338 Match: biding and action points (bide, extra actions, AP-cost abilities, give-AP), campaign only`.

---

### Task 6: Freeing, the boss win, game over and the rite (#339)

**Files:** `Fight.cpp`, `Turn.cpp`, `Battle.cpp`, `Abilities.cpp`, `Match.hpp`, `Match.md` ("Freeing and the end"), `match-words.json`, `tests/MatchFightTests.cpp`.

**Interfaces:**
- `ActionKind::Cut` (a = unit): `Check` — the actor has `flag::Player` (`OnlyPlayer` otherwise), the target is `Fallen`, has `flag::Strung` (`NotStrung`), is `Beside` the player; it is the turn's action (`AlreadyActed`). `Apply`: clear `Strung`, set `Freed`, `cell = -1`, log `LogKind::Freed` (a = player, b = unit, c = its cell), `Acted`, then `CheckEnd`.
- `void CheckEnd(const MatchDef &def, MatchState &s);` — called after every `Down`, removal and `Cut`. Campaign (`win == Boss`), in this order: (1) `playerLoss` and the player fallen → `over`, `winner = 1`, `loss = PlayerFell`; (2) no side-0 unit `Up` → `SideDown`; (3) `mustFree` and a recruit `Removed` while strung, or every boss down while a recruit is still `Strung` → `Trapped`; (4) every `Boss` unit fallen or removed (and, with `mustFree`, every recruit `Freed`) → `over`, `winner = 0`. Log `Over`. The faction mode keeps its round-end scoring.
- `ActionKind::Rite`: `Check` — `over` and `loss == PlayerFell` and `riteReady` and not `riteUsed` (else `NoRite`). `Apply`: every side-0 unit that is fallen stands up at full HP (placed by `RevivePlace` with the player as reviver), every standing side-0 unit healed to full; `ap` stays 0 for the revived; `riteUsed = true`, `riteReady = false`, `over = false`, `winner = -1`, `loss = None`; a battle in progress ends; `NextTurn` from where the order stood. (Plan 5 saves `riteUsed` at once and sets `riteReady` after T3.)
- `Current` while `over` and a rite is possible: the player (so the app and bots can offer `Rite`); otherwise -1 as today.

- [ ] **Step 1: Failing tests:** `"Only the player cuts, beside a fallen strung recruit, as the turn's action"`; `"The fight is won when the boss falls and, if the stage asks, the recruit is freed"` (cut then boss down → won; with `mustFree` off, boss down alone → won); `"The boss falling while the recruit is strung loses the fight (trapped)"`; `"A removed strung recruit loses a fight that must free it"`; `"The player falling ends the fight at once, even in an enemy's battle"` (Review Focus 4); `"The whole side down is a loss"`; `"The rite stands the side up and the fight goes on; once used it is gone, even after another loss"` (a second loss → `Rite` is `NoRite`, `riteUsed` still true); `"No rite for a trapped loss or when not ready"`.
- [ ] **Step 2:** Run — FAIL.
- [ ] **Step 3: Implement** `Fight.cpp`, the calls to `CheckEnd`, `Current` for the rite.
- [ ] **Step 4:** `Umbra.Match` — PASS.
- [ ] **Step 5: Commit** `issue #UMBRA-339 Match: the gold knot and freeing, the boss win and its extra condition, game over, the rite`.

---

### Task 7: Records and the whole check (#340)

**Files:** `docs/testing/s46-campaign-rules.md` (ROADMAP §14 template), `docs/ROADMAP.md` (S46 row), `docs/current-engine-status.md`, `CLAUDE.md` (first-use paragraph: plan 2 done, where the modes live), the handoff in the Project (§0 "Where the work stands").

- [ ] **Step 1:** Full build and every CTest (`ctest`, 59 suites with the balance soaks) — PASS; record the soak's numbers before and after Task 2.
- [ ] **Step 2:** Clang build of the match tests and a GCC ASan run of `UmbraMatchTests` (the doc's recipe) — PASS, or record what can't run.
- [ ] **Step 3:** Write the record: what changed, the tests, the soak, a capture of a faction match after the order change, and §4 for the user: play a faction match on Windows and say whether it feels different with monsters in the order (the balance numbers hold, the feel is the user's).
- [ ] **Step 4:** Ship the commits to the PC (bundle, cherry-pick as the user, compare trees).
- [ ] **Step 5: Commit** `issue #UMBRA-340 Campaign plan 2 recorded: the fight rules (modes, one Speed order, falls, biding, freeing, the rite); faction soak holds`.

---

## Left to plan 4 and later (not this plan)

Monster behaviours (aggressive after n rounds, passive, reinforcement), monsters biding, enemy scaling, stage files and their `mustFree`, the Rat's kit and ticket windows (plan 4); saving the rite and setting `riteReady` after T3, the game-over screen, retries, items kept between stages, 4 slots and swap on a square (plan 5); drawing falls, strings, the knot and AP pips (plan 7).
