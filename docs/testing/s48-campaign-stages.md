# The Last Curtain, campaign plan 4: fights you can play — verification record

## Record

- Date: 2026-11-24 (overnight, unattended; the user's instruction "proceed with all the next plans").
- Commits: `issue #UMBRA-352` to `#UMBRA-364` (the art commits `#UMBRA-358`, `#UMBRA-360` sit between).
  Plan: `docs/design/plans/2026-11-23-campaign-plan-4-stages.md`; spec: `docs/design/2026-11-23-campaign-slice-1.md`
  §7.3–7.5, `docs/design/2026-11-23-campaign-ui-handoff.md` §2 and §9, `docs/design/2026-11-23-ti-content.md`.
- Tester: Claude (remote session), Linux cloud workspace, GCC 13 Debug; SwiftShader under `xvfb-run`.
- State: **implemented/unverified** on Windows; every new Vietnamese line is a **draft** for the user's review.

## 1. What changed

- **Match rules:** biding off in the campaign mode; abilities `hasten` (Đưa tin: the ally's extra full turn
  right after Tí), after-hasten (Chân sáo: +3 move next turn), effects (mark, alarm, first shot, steal and the
  thief's stash, dart, speedkill, death paper, cold rice, footsteps); item use flags and families
  (ingredients and keepsakes go to `found`, the café's storage); drop tables rolled from the fight's seed;
  reinforcements marked a round ahead; gimmicks (hazards, breath rows, windows that let specters out, dark),
  each marked a round before it acts; a boss still waiting to arrive keeps the fight going. Save version 5.
- **`modules/campaign`:** `growth.csv` and `MakeUnit` (zodiac ×1.33, ghosts ×0.6, kits); stage files
  (`assets/campaign/stages/*.json`, validated: file, field, fix); `BuildStage`; behaviours (aggressive after n,
  passive, reinforcement); `EnemyAction`, `PartyAction` (the party as a bot); `StageMoments` (scenes at
  start, round:n, firstAttacked/falls/freed, end or lost, each once, only when told); `Sync` for spawned
  units; the soak (`CampaignSoak.hpp`, `UmbraTool campaign-soak`, `balance.json`).
- **The slice's content:** T1 Khách lạ, T3 Hành lang chân dung, Stage 1 Phòng vé (Phòng két holds the
  keepsake), skirmishes Kho đạo cụ, Cầu thang phụ, Phòng vé (three twists each); 13 scenes (drafts); items
  Gừng, Đường, Mẩu giấy gấp tư; cast rows for the kits and a second thief.
- **The app:** `MatchPlay::Pause/Quiet/Told`; `LastCurtain --stage <id> [--refill n] [--seed n]`
  (`StageApp.cpp`: the match screen with the enemy brain and the stage's scenes on the dialog screen);
  `build stage <id> [n]`; six "Play" entries in `projects/reference/project.json`.

## 2. Checks (Linux)

- `Umbra.Match` (129 cases), `Umbra.Campaign` (27 cases, 16,806 assertions): every rule above has its test;
  every shipped stage and scene loads; the party brain wins every stage and refill on some seed; every cast
  member can strike in a battle.
- `UmbraTool campaign-soak` on 200 seeds per stage and refill: the bands hold (table in `Campaign.md`
  "The soak"); the `[campaign-balance]` test (`Umbra.Campaign.Balance`) checks them all.
- Full run: `cmake --build build-gcc` (BUILD_RC=0, no warnings), then `ctest -E Balance -j4`: **59/59
  passed**. Balance tests: see §2a.
- Pictures (SwiftShader): `LastCurtain --stage stage1 --seed 3 --capture ... --frames 90` (the board with the
  party on its starts) and `captures/2026-11-24-stage-t3.png` (`--stage t3 --seed 2 --frames 200`, a battle).

### 2a. Balance

`ctest -R "Umbra.Match.Balance|Umbra.Campaign.Balance"`: **2/2 passed** (the faction soak 1,170 s, unchanged by the campaign additions; the campaign soak 133 s, every stage and refill within its band).

**Not run:** Clang and ASan; MSVC; the scenes during a stage in a window (a capture skips them; `StageMoments`
and `Pause/Quiet` are tested headless).

## 3. Limits (owned by later plans)

- The stage screen is the faction match's HUD: units show faction names and figures (Huy, Lan, "Dinh Quoc Bao
  (theirs)" for the Keeper) and English words — plan 5 (battle HUD and words).
- The table draws the campaign map with the faction match's look; walls and windows are not drawn apart yet
  — plan 5.
- No stage save, party or XP between fights; `--stage` uses the player, Bảo and Bà Mai at the stage's level
  (+ the refill number) — plan 7.
- Backdrops `prop_store` and `back_stairs` do not exist yet (no scene uses them).
- Trapped losses and play minutes are reported, not banded (the bot is careful and fast).

## 4. Open: the user's checks

1. **Read the drafts** (`plugins/last-curtain/assets/campaign/scenes/t1-*.json`, `t3-*.json`, `stage1-*.json`):
   Tí's lines are from `ti-content` §4.1 with "con Chuột" for "Tý"; Lệ Thanh's story piece 1 (money, the
   jade bracelet) in `stage1-end.json` is new. Change freely; only the file names are used by the stages.
2. **Play** `build stage t1`, `build stage t3`, `build stage stage1`, `build stage sk_props 1`: the scenes
   come at their moments and the fight waits; Tí must fall and be cut free before the Rat falls; windows
   are marked a round before a specter steps out; the Keeper's row is marked before the breath.
3. Is Stage 1 right at the Rat's level 2 (bots win 45%), or back to level 3 (bots 24-42%)?

Result: **not run**.
