# Campaign plan 4: stages, enemies and the slice's fights — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stage files that build a campaign fight on the plan-2 rules: hand-drawn maps, the party, enemies built from a class at a level and scaled, three behaviours, items on squares, gimmicks and in-fight scenes; then the slice's content (T1, T3, Stage 1 and two skirmish rooms), the Rat's kit, and a campaign soak.

**Architecture:** `umbra::campaign` gains `StageDef` (loaded and validated from `assets/campaign/stages/<id>.json`) and `BuildStage(def, cast, stage, party, seed) -> MatchState`, which draws the map into the existing `match::Board`, places units with flags, sets `mode` and calls `BeginMatch`. Enemies are side 1 characters (scaled class lines) driven by the faction bots' `ChooseAction` (the user, 2026-11-23), steered by each unit's behaviour. Gimmicks and new ability effects are small, named rules in `match` (data says where and how often). Scenes from plan 3 fire at fight moments and pause the fight.

**Tech Stack:** C++20, Catch2, nlohmann/json; `match`, `matchbots`, `campaign`; the app's `MatchPlay`.

**Spec:** `docs/design/2026-11-23-campaign-slice-1.md` §7.4, §7.5, §7.11; `campaign-content-2026-11-22.md` (the Rat, Tí); `cast-2-2026-11-22.md` (Bảo and Bà Mai's kits); the user's answers of 2026-11-23 evening (enemies use the faction bots; skirmish rooms on the theatre map, XP capped at the next stage's level, refilled after each story stage with a new twist).

## Global Constraints

- Every number and word is data. Campaign maps are **not symmetric**; the mirror fairness check is off for them.
- Enemies: a class line at a level, then scaled: **zodiac** attack and HP **×1.33** (rounded), other stats as the class; **ordinary ghost ×0.6**. A level-3 attacker has 6 attack; the Rat (level-3 fighter) 8; a ghost about 4. Ordinary ghosts are drawn at height **0.8**.
- Behaviours (per unit): **aggressive after n rounds**; **passive** (attacks only what it sees: same room, up to **4** squares); **reinforcement** (appears at round n on a named square, the "spawns soon" marker one round ahead).
- Sides: party 0; enemies 1, hostile to 0 only; friendly and neutral ghosts on their own sides.
- Stage 1: **the Rat never bides; nobody on his side revives**; ticket windows open and close each round and let out rat specters; a stolen item sits on a window until the Rat is hit, then drops on his square for anyone to pick up. **Tí freed and the Rat defeated** wins; the Rat falling with Tí strung loses.
- Play length targets for the soak: **T1 ~5 min, T3 ~10, Stage 1 15–20**. No round limit except as a stage's aim; the soak alone caps rounds.
- XP per **stage won**, set in the stage file, about one level a stage; recruits join at a fixed level (**Tí 3; Bảo and Bà Mai 2**).
- Content design rule (the user, 2026-11-23 evening): **every fight teaches or twists exactly one new thing and recombines the old ones**; no two fights in a row ask the same question.
- Commits from **#348**.

## The slice's fights (content; Vietnamese drafted in the stage files for the user's review)

| Fight | Map | New idea | Recombines | Win | Notes |
|---|---|---|---|---|---|
| **T1 Khách lạ** (foyer, today) | The grand foyer: a wide hall, the twin staircase, velvet rope lanes; 9×7 | Moving, attacking, reading the order strip | — | the three rat specters down (one "boss" flag on the last) | Level 1, the player alone. Specters rise one at a time from the rope posts (reinforcement rounds 1, 2, 4) so the player meets one at a time; the third carries a red string to the ceiling the player can see but not cut yet (foreshadows the knot). Tuned so falling is unlikely. Ends with Lệ Thanh's rescue scene: she touches the strings and they go slack. |
| **T3 Hành lang chân dung** (portrait corridor) | A long corridor, 13×5, alcoves on both sides, portraits on the walls | **Biding**, falls, reviving, game over (and the rite unlocked after) | moving and fighting from T1 | the Portrait Keeper (mini-boss) down | Level 2, the player + Bảo + Bà Mai. **The Keeper telegraphs**: every other round it "draws breath" (a scene beat and a mark under it), then strikes the whole lane in front of it next round. The lesson: bide while it breathes, step out of the lane, then spend 2 AP for a three-action burst. Two specters in the alcoves are passive until someone walks past. Bảo's *Chẩn đoán* marks the Keeper; Bà Mai's cold rice revives whoever the lane strike drops. If the player falls: the game-over screen (Thử lại only once the café is unlocked? no: after T1, all three choices), and after the fight Lệ Thanh's scene unlocks the rite. |
| **Stage 1 Phòng vé** (box office, the Rat) | The box office: a counter with three ticket windows across the middle, the queue rails, a back room where Tí hangs; 11×9 | **Freeing a recruit under time pressure; theft** | biding (the Rat's darts reward stored AP), reviving, reinforcements | Tí freed **and** the Rat down | Level 3, the player + Bảo + Bà Mai. **The tension**: hurt the Rat too fast and he falls with Tí strung (lost), so the party must reach Tí first, drop him (he fights, strung), and cut him, while the Rat steals and darts. **Windows**: each round one or two of the three open (a pattern from the file, marked a round ahead); an open window lets out a rat specter (reinforcement) and is where a stolen item waits. **Móc túi** (steal): the Rat takes a random item from an adjacent unit; it sits on the window nearest him until he is hit, then drops on his square. **Chạy làng** (dart): after a battle the Rat moves up to 3 squares away. **Speedkill**: his strikes deal double to an attacker slower than him (Bảo and Mai are). The first revive item (a bandage that stands a fallen ally up) lies on the counter. The in-fight line when he is first attacked: "Tao cần tiền, Lệ Thanh, mau đưa tiền cho tao!" Ends: Lệ Thanh's story piece 1 (money) and she regains restoring memories. |
| **Skirmish: Kho đạo cụ** (prop store) | Shelving aisles, 9×9 | **Hazards**: sandbags marked a round ahead fall on their squares | everything so far | every enemy down | Optional, on the theatre map. Twists after each story stage won (refill): after Stage 1 → "rats in the walls" (a reinforcement every 3 rounds) |
| **Skirmish: Cầu thang phụ** (back stairs) | A narrow two-landing stair, 5×11 | **A pincer**: enemies come from both ends | biding (the narrow stair blocks), reviving | every enemy down | Optional. Twist after Stage 1 → "lights out" (passive enemies see only 2 squares; the player's side also sees less: the light rule) |

Skirmish rewards: XP until the party reaches the **next story stage's recommended level** (the user), then none; map items taken once per refill; plan 5 shows them on the map.

**The escalation ladder beyond the slice** (for later stages; recorded in `Campaign.md` so later content keeps the rule): stage 2 adds a second recruit whose freeing helps (his knot cut early gives a buff); stage 3 turns `perform` and `respawn` back on as a gimmick (the spec's "one later stage"); later bosses bide and take three-action turns; zodiacs drawing power from strings (cutting weakens them).

---

### Task 1: The cast sheet's stats and enemy scaling (#348)

Files: `assets/campaign/cast.csv` (+ `role,level,hpPct,attackPct` columns, `kit` = ability ids), `assets/campaign/growth.csv` (`role,level,hp,attack,defence,speed,move`: the class line at each level 1–15), `modules/campaign` (`EnemyUnit(def, cast, id, level) -> match::Unit` with the scaling above). Tests: the Rat at level 3 has 8 attack and ~40 HP; a level-3 ghost attacker ~4 attack; a level-1 player matches the All-Rounder line. Commit.

### Task 2: Stage files (#349)

`StageDef` (id; `title` vi and `titleEn`; level; backdrop; music; xp; `map` rows; `party` and start squares; `units` (who, square, side, nature, flags `boss`/`recruit`, height, behaviour with its numbers); `items` on squares; `mode` name plus overrides (e.g. `mustFree`); `gimmicks` with numbers; `aims` (start hint, teaching beats, recap); `scenes` (moment → scene id: `start`, `end`, `firstAttacked:<unit>`, `falls:<unit>`, `round:<n>`, `bossBelow:<pct>`, `freed:<unit>`)). Map characters: `#` wall, `.` floor, `~` hazard square, `W` window, `R` rope post (a spawn point), `S` party start; doors are open between neighbouring floor squares. Validation names file, field and fix (unknown cast id, a unit on a wall, two units over capacity, a scene id with no file, a window gimmick without `W` squares…). Tests per rule. Commit.

### Task 3: Building a fight (#350)

`BuildStage(...) -> match::MatchState`: the map into `Board` (cells, doors from neighbours; `Fair` not asked), the party from the save's characters (plan 5 passes them; here a test party), enemies by `EnemyUnit`, flags, items as `Floor`, mode with overrides, `BeginMatch`. Reinforcements are pre-made units off the board with their round. Tests: T1's file builds; units where the file says; the mode campaign with `mustFree` in Stage 1. Commit.

### Task 4: Behaviours and the enemy brain (#351)

Side 1 acts through `matchbots::ChooseAction`, filtered by the unit's behaviour: **passive** — `EndTurn` unless a party unit is in the same room within 4 squares (or it was attacked); **aggressive after n** — holds until round n, then the bot plays; **reinforcement** — placed at round n on its square if free (else the nearest free), `SpawnSoon` logged a round ahead, joins the order next round. A `bides` flag lets a unit bide when the bot has nothing worth doing (the Rat's is off). `MatchPlay` asks the brain for side 1 as it does for the faction match's rival side. Tests: each behaviour; the Rat never bides; nobody on his side revives (the stage's `noRevive` side flag). Commit.

### Task 5: New effects and the Rat's kit (#352)

`abilities.csv` gains `effect` (`none`, `mark`, `steal`, `dart`, `speedkill`, `deathpaper`, `riceRevive`) and the campaign kits: Bảo (*Dao mổ*, *Chẩn đoán* mark +1 damage taken until the target's next turn, *Giấy chứng tử* heal 2 when a foe falls to him), Bà Mai (*tend*, *mend*, *Cơm nguội*: heals 5 and cleanses one status, or revives a fallen ally at 30%), the player's All-Rounder kit with Lệ Thanh's fan, the Rat (*Móc túi*, *Chạy làng*, *Speedkill*), Tí's (*Ná cao su*, *Đưa tin* give-AP, *Chân sáo* +3 move after a bide; used once he joins, plan 6). Names in `campaign-words.json` (vi, en). Tests per effect (mark expires; steal parks on the nearest open window, drops when the Rat is hit; dart moves away up to 3; speedkill doubles only against a slower attacker; death paper heals on a kill; cold rice heals or revives). Commit.

### Task 6: Gimmicks (#353)

`windows` (Stage 1: a pattern of which windows open each round, marked a round ahead; an open window spawns a rat specter if its square is free, at most n alive), `hazards` (skirmish: k marked squares each round fall next round for x damage), `ropes` (T1: reinforcement posts), `breath` (T3: the Keeper's telegraphed lane strike every other round, its mark shown a round ahead). Each is a small function in `match` called at the round's start, with data from the stage file. Tests per gimmick. Commit.

### Task 7: Scenes inside a fight (#354)

`StageMoments(state, log since last check) -> scene ids` (each moment once per attempt); `MatchPlay` queues a scene, waits for the current beat to finish, pauses the fight, shows `DialogView`, resumes where it was. Tests: a moment fires once; the fight state is unchanged by the scene; a scene during a battle waits for the battle's beat. Commit.

### Task 8: The slice's content (#355)

`stages/t1.json`, `t3.json`, `stage1.json`, `skirmish-props.json`, `skirmish-stairs.json` with their twists by chapter; scenes `t1-start`, `t1-rescue`, `t3-start`, `t3-keeper-breath`, `t3-end-rite`, `stage1-start`, `stage1-rat-first-hit`, `stage1-ti-falls`, `stage1-freed`, `stage1-end` (Lệ Thanh's story piece 1, money), `stage1-trapped`; all Vietnamese, drafted for review, with `{player}` and `{anh/chị}`. Tests: every file loads; each stage can be won by a scripted line of actions (a test plays it). Commit.

### Task 9: The campaign soak (#356)

`UmbraTool campaign-soak <stage> [--seeds 200]`: bots play the party too; report wins, falls per unit, rite uses, turns and an estimated play time (turns × seconds a turn from options), rounds capped in the soak only. Bands in `assets/campaign/balance.json` (T1 player falls ≤ 5%; T3 wins 60–85% without the rite; Stage 1 wins 45–70%, trapped losses 10–25% so the tension is real; play times near the targets). Tune the stage data until the bands hold; record the tuning in `Campaign.md`. Commit.

### Task 10: Records (#357)

`docs/testing/s48-stages.md` (§4: play T1, T3 and Stage 1 on Windows — once plan 5's flow exists, or through `LastCurtain --stage <id>`), ROADMAP, status. Ship.
