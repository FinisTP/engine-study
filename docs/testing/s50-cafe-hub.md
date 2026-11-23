# The Last Curtain, campaign plan 6: the café hub, brewing memories, the notebook — verification record

## Record

- Date: 2026-11-24 (overnight, unattended).
- Commits: `issue #UMBRA-375` to `#UMBRA-381`, records `#UMBRA-382`. Plan: `docs/design/plans/2026-11-23-campaign-plan-6-cafe.md`;
  spec: `docs/design/ux/cafe.md`, `ux/notebook.md`, `docs/design/2026-11-23-ti-content.md` §2–7.
- Tester: Claude (remote session), Linux cloud workspace, GCC 13 Debug; SwiftShader under `xvfb-run`.
- State: **implemented/unverified** on Windows; every new Vietnamese line is a **draft**.

## 1. What changed

- **`campaign` (`CampaignCafe.hpp`):** the campaign save in memory (storage, recipes, taken ids, memories,
  talks, recruits, facts, chosen versions, boosts, slots, party); `recipes.json` (Mứt gừng, Nước gừng);
  `Lacks` (what is missing and in which stages it is found, "?" when none), `Brew`, `SlotOk` (ingredients and
  keepsakes never take a battle slot); `memories.json` (Tí's four: talk, Mứt gừng, the folded note, Nước dừa
  locked), `NextMemory`, `Remember` (joins, the answer picks the upgrade version once), `AbilityFor`; general
  drinks `Boost`/`Boosted` with `options.csv` `boostCapPerStat` 3; talks (`talks/*.json`: after a won stage,
  facts all/none, sets, teaches, memory, repeat), `OpenTalks` (≤ 3 new), `RecordFight` (won:, fell:,
  ratStole:).
- **The app:** `CafeHub` (people with a new-talk mark, recipes with lacks and the two versions, keepsakes to
  give, toasts), `Notebook` (Nhân vật, Nguyên liệu, Kỷ vật; "?" for the unknown; slots edited outside battle
  only), screens `cafe-hub`, `brew`, `notebook`; `LastCurtain --campaign-cafe` (a sample save after Stage 1).
- **Content (drafts):** Tí's memory scenes 1–4 (`ti-memory-*`), idle lines (`ti-idle-*`), six talks for Lệ
  Thanh, Bảo and Bà Mai; Lệ Thanh teaches the two ginger recipes with Tí's first memory.

## 2. Checks (Linux)

- `UmbraCampaignTests`: 33 cases — recipes and lacks with where; brewing spends; slots; memories (join, both
  versions, final, locked); boosts to the cap; talks (opening, limit, facts, a memory talk, teaching, idle
  lines after joining); the café hub (talk → memory 1 → Tí joins, brew → memory 2 → version, keepsake →
  memory 3, a boost, lacks after); the notebook (tabs, "?", reopening where left, slots, read-only).
- `Umbra.Tool.CafePreview` (new): `ui-preview cafe-hub brew notebook` with `cafe-campaign-sample.json`:
  0 errors, 0 warnings. Every letter of the café's Vietnamese screens is in the fonts (`ArtTests`).
- Full run: see §2a. Pictures: `captures/2026-11-24-campaign-cafe.png`, `captures/2026-11-24-notebook.png`.

### 2a. Full run

`cmake --build build-gcc` (BUILD_RC=0, no warnings), then `ctest -E Balance -j4`: **60/60 passed** (the new `Umbra.Tool.CafePreview` makes 60). No rule or stage data changed, so the soaks were not re-run.

**Not run:** Clang and ASan; MSVC; clicking through talks, brewing and the notebook in a window.

## 3. Limits

- The café is a screen hub over the back-wall picture, not yet the walking room (plates within 3 m, E).
- The save lives in memory; plan 7 writes it, carries it into stages (party, slots, boosts, versions) and
  records each fight with `RecordFight`. Until then `--stage` uses base kits and `--campaign-cafe` a sample.
- The notebook in battle (read-only, fight paused) is tested headless but not wired (J still pauses).
- Task 7 (the model rewording talk lines) is deferred; lines play as written.

## 4. Open: the user's checks

1. Read the drafts: `scenes/ti-memory-1..4.json`, `ti-idle-*.json`, `talk-*.json`. Are Tí's voice and the
   choices right? Which answer should give which version (`memories.json`)?
2. `LastCurtain --campaign-cafe`: talk to Lệ Thanh twice (her first talk, then Tí's memory 1: he joins),
   brew Mứt gừng (memory 2, pick a version), give the folded note (memory 3), give Nước gừng to someone until
   it is refused at 3, open Sổ tay and fill a slot.

Result: **not run**.
