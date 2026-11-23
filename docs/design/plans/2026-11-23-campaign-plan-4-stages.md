# Campaign plan 4: fights you can play (rules additions, stages, the slice's fights) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stage files that build a campaign fight on the plan-2 rules, the rule additions of the UI
handoff, and the slice's fights (T1, T3, Stage 1, three skirmish rooms), playable from the editor's
Play and `build stage <id>`, balanced by a campaign soak.

**Architecture:** `umbra::campaign` gains `StageDef` (`assets/campaign/stages/<id>.json`, validated) and
`BuildStage(def, cast, stage, party, refill, seed) -> MatchState`, which draws the map into the existing
`match::Board`, places units with flags, sets the mode and calls `BeginMatch`. Enemies are side 1
characters (scaled class lines) driven by the faction bots' `ChooseAction`, steered by each unit's
behaviour. New effects and gimmicks are small named rules in `match`; numbers are data.

**Tech Stack:** C++20, Catch2, nlohmann/json; `match`, `matchbots`, `campaign`; the app's `MatchPlay`.

**Spec:** `docs/design/2026-11-23-campaign-slice-1.md` §7.3–7.5; `docs/design/2026-11-23-campaign-ui-handoff.md`
§2 (newer, wins); `docs/design/2026-11-23-ti-content.md`; the user's answers of 2026-11-23/24 below.

## Decisions this plan carries (the user, 2026-11-23 evening and 2026-11-24)

- **Biding is off** in the campaign mode for now (`rules.json` campaign `"biding": false`). No AP is
  earned; AP costs, extra actions and give-AP stay in the code, unused. No AP pips, no "+1 AP".
- **Đưa tin** (Tí, 2 HP, range 6): the chosen ally (never Tí) takes **a full extra turn (move +
  action) right after Tí**, and still acts at its usual place in the order. Once per Tí turn.
  New ability effect `hasten`.
- **Chân sáo** (Tí's passive): **+3 move on his next turn after he used Đưa tin.**
- **Stage 1:** the win needs Tí freed and the Rat down; the Rat falling with Tí strung loses (as built,
  `mustFree`). Tí's "left strung" recap line is dropped; the result screen's recap line is for later
  recruits.
- **The keepsake Mẩu giấy gấp tư** lies in **a separate side room** of the Stage 1 map (Phòng két, the
  cashier's back room, through a door behind the counter): taken by stepping onto its square; it goes to
  the café's storage, never a battle slot; a lost stage loses it.
- **Gừng** drops (30%) from rat specters in Stage 1 and in **a Phòng vé skirmish** (the box office map
  with a twist, opening after Stage 1). **Đường** lies on one square in T3, once.
- **Skirmish rooms refill after each story stage won, each time with a new twist** (the user's earlier
  rule, kept over the spec's "used up"). XP from skirmishes stops at the next story stage's
  recommended level; drops still come. A lost skirmish keeps nothing.
- **Party size per stage** is in the stage file.
- **Item use flags:** every item lists where it can be used (`cafe`, `board`, `fight`). Ingredients and
  keepsakes are café-only and go to the café's storage when picked up.

## Global Constraints

- Every number and word is data; C++ literals ASCII. Campaign maps are not symmetric (the mirror check off).
- Enemies: a class line at a level, then **zodiac attack and HP ×1.33** (rounded), **ordinary ghost ×0.6**,
  ghosts drawn at height 0.8.
- Behaviours: **aggressive after n rounds**; **passive** (acts only on a party unit in its room within 4
  squares, or once attacked); **reinforcement** (appears at round n on a named square, marked a round ahead).
- Content rule (the user): **every fight teaches or twists one new thing and recombines the old ones.**
- Play length targets for the soak: T1 ~5 min, T3 ~10, Stage 1 15–20. Commits from **#352**.

## The slice's fights

| Fight | Map | New idea | Win | Notes |
|---|---|---|---|---|
| **T1 Khách lạ** (foyer, today) | 9×7, twin stairs, rope lanes | moving, attacking, the order strip | the three rat specters down | Party: the player. Specters rise one at a time from rope posts (rounds 1, 2, 4); the third trails a red string (foreshadows the knot). Ends with Lệ Thanh's rescue scene. |
| **T3 Hành lang chân dung** | 13×5, alcoves, portraits | **reading a telegraph; falls and reviving**; the rite unlocked after | the Portrait Keeper down | Party: player + Bảo + Bà Mai, level 2. Every other round the Keeper "draws breath" (marked) and strikes its whole lane next round: step out of the lane, then punish. Two alcove specters are passive until passed. Bảo's Chẩn đoán marks; Bà Mai's cold rice revives whoever the lane drops. Đường on one square. |
| **Stage 1 Phòng vé** | 11×9: counter with three windows, queue rails, Tí's back room, **Phòng két** (side room, the keepsake) | **freeing a recruit under pressure; theft** | Tí freed **and** the Rat down | Party: player + Bảo + Bà Mai, level 3. Windows open by a pattern (marked a round ahead), each lets out a rat specter (Gừng 30%) and holds what the Rat stole. Móc túi steals from a neighbour (parked on the nearest window until he is hit, then dropped on his square); Chạy làng darts 3 squares after a battle; Speedkill doubles his strikes on a slower attacker. A bandage on the counter. Line on his first hit: "Tao cần tiền, Lệ Thanh, mau đưa tiền cho tao!". Ends with Lệ Thanh's story piece 1 (money). |
| **Skirmish Kho đạo cụ** | 9×9 aisles | hazards: sandbags marked a round ahead | all down | Twists by refill: 1 sandbags only; 2 "chuột trong tường" (a reinforcement every 3 rounds); 3 both. |
| **Skirmish Cầu thang phụ** | 5×11, two landings | a pincer from both ends | all down | Twists: 1 pincer; 2 "tắt đèn" (enemies see 2 squares); 3 a mini-boss at the top. |
| **Skirmish Phòng vé** (after Stage 1) | the box office map | the windows without the Rat | all down | Gừng drops; twists: 1 two windows; 2 three windows faster; 3 a second thief. |

Tí's in-fight lines (from `ti-content` §4.1, "Tý" → "con Chuột" in Lệ Thanh's hint): first seen, hit
once, falls strung, cut free.

---

### Task 1: Rule additions (#352)

`rules.json` campaign `biding: false`; ability effect `hasten` (target an ally within range, not self;
the ally is put next in the order for one extra full turn, keeps its own; once per user turn); Tí's
passive `afterHasten` (+3 move on his next turn after `hasten`); items gain `use` (list of `cafe`,
`board`, `fight`; `UseItem` refuses outside them, `Why::NotHere`) and `kind` (`item`, `ingredient`,
`keepsake`); picking up an ingredient or keepsake goes to `MatchState::found` (not a slot, never refused
for full slots); drop tables per enemy (`drops: [{item, percent}]`) rolled from the match's seed at the
fall. Tests: hasten's extra turn and the ally's own turn both happen; not on self; once per turn; the
passive's move only on the next turn after hasten; use flags refuse a café item in a fight; found items
bypass slots; drops are the same for the same seed and roughly the percent over 2,000 falls. Faction soak
unchanged (its mode has none of these). Commit.

### Task 2: Cast stats and enemy scaling (#353)

`cast.csv` gains `role,level,kit`; `growth.csv` (`role,level,hp,attack,defence,speed,move`, levels 1–15);
`EnemyUnit(def, cast, id, level)` with the scaling. Tí's row as `ti-content` §1 (Speedster, HP 16, Atk 4,
Def 2, Spd 6, Move 12 at level 3). Tests: the Rat at level 3 has 8 attack; a level-3 ghost about 4; Tí's
numbers. Commit.

### Task 3: Stage files (#354)

`StageDef`: id, titles (vi/en), recommended level, party cap, backdrop, music, xp; `map` rows (`#` wall,
`.` floor, `~` hazard, `W` window, `R` rope post, `S` party start, `D` a door between rooms); `units`
(cast id, square, side, flags boss/recruit, height, behaviour and numbers, drops); `items` on squares
(items, ingredients, keepsakes; take-once ids); mode overrides (`mustFree`); `gimmicks`; `aims` (start
hint, recap); `scenes` (moment → scene id: start, end, firstAttacked:<unit>, falls:<unit>, round:<n>,
freed:<unit>); for skirmishes `twists` (one entry per refill: overrides of units and gimmicks).
Validation names file, field and fix (unknown cast id, a unit on a wall, a window gimmick without `W`,
a scene with no file, more party starts than the cap…). Commit.

### Task 4: Building a fight (#355)

`BuildStage(def, cast, stage, party, refill, seed)`: the map, doors from `D` and neighbours, the party
up to the cap, enemies by `EnemyUnit`, items not yet taken (the save's list, passed in), the twist for
`refill`, mode overrides, `BeginMatch`. Tests: T1 builds as written; Stage 1 has `mustFree`; a taken
item is absent; refill 2 of a skirmish has its twist. Commit.

### Task 5: Behaviours and the enemy brain (#356)

Side 1 acts through `matchbots::ChooseAction`, filtered by behaviour (passive, aggressive after n,
reinforcement with its mark a round ahead). The stage's `noRevive` side flag (nobody on the Rat's side
revives). `MatchPlay` asks the brain for side 1. Tests per behaviour. Commit.

### Task 6: Effects, kits and upgrade versions (#357)

`abilities.csv` `effect`: `mark` (+1 damage taken until the target's next turn), `steal`, `dart`,
`speedkill`, `deathpaper` (heal 2 on a kill), `riceRevive` (heal 5 and cleanse, or revive at 30%),
`hasten`; kits for the player (the fan), Bảo, Bà Mai, the Rat, the Keeper, Tí (Ná cao su, Đưa tin, Chân
sáo). **Upgrade versions** as ability variants chosen later in the café (plan 6): Ná cao su **Báo động**
(target −2 Speed until its next turn) / **Bắn trước** (+3 power against a unit that hasn't acted this
round); Đưa tin **Về kịp** (the ally also +2 move) / **Phần quà** (the ally also heals 3); Chân sáo
**Gió biển** (+4 instead, hazards cost nothing extra) / **Dấu chân** (allies ending their move next to
him get +1 move next turn). A unit's chosen version comes from the party passed to `BuildStage`. Names in
`campaign-words.json`. Tests per effect and version. Commit.

### Task 7: Gimmicks (#358)

`windows` (pattern, marked a round ahead, spawns, stolen items parked), `hazards`, `ropes`, `breath` (the
Keeper's lane strike every other round, marked a round ahead), `dark` (sight radius for passive
enemies). Small functions in `match` at the round's start. Tests per gimmick. Commit.

### Task 8: Scenes inside a fight (#359)

`StageMoments(state, log since last check) -> scene ids`, each once per attempt; `MatchPlay` waits for
the beat, pauses, shows `DialogView`, resumes. Tests: once only; the state is unchanged by a scene; a
scene during a battle waits. Commit.

### Task 9: The slice's content (#360)

Stage files above; scenes `t1-start`, `t1-rescue`, `t3-start`, `t3-breath`, `t3-end-rite`, `stage1-start`,
`stage1-rat-first-hit`, `stage1-ti-*` (Tí's four lines), `stage1-freed`, `stage1-end` (story piece 1),
`stage1-trapped`; Vietnamese drafted for review with `{player}` and `{anh/chị}`. `project.json` `"play"`
gains a start per stage; `build.bat stage <id>`. Tests: every file loads; each stage can be won by a
scripted line of actions. Commit.

### Task 10: The campaign soak (#361)

`UmbraTool campaign-soak <stage> [--seeds 200] [--refill n]`: bots play both sides; wins, falls per unit,
trapped losses, turns, estimated minutes, Gừng found. Bands in `assets/campaign/balance.json` (T1 player
falls ≤ 5%; T3 wins 60–85%; Stage 1 wins 45–70% with trapped losses 10–25%; skirmishes 70–90%). Tune the
data; record it in `Campaign.md`. Commit.

### Task 11: Records (#362)

`docs/testing/s48-fights.md` (§4: play T1, T3, Stage 1 and a skirmish on Windows from the editor's Play
or `build stage <id>`), ROADMAP, status. Ship.
