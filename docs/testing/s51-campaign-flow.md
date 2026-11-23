# The Last Curtain, campaign plan 7: the save, the map, around a stage and the flow — verification record

## Record

- Date: 2026-11-24 (overnight, unattended).
- Commits: `issue #UMBRA-383` to `#UMBRA-387`, records `#UMBRA-388`. Plan: `docs/design/plans/2026-11-23-campaign-plan-7-around-a-stage.md`;
  spec: `docs/design/ux/world-map.md`, `ux/stage-flow.md`, `ux/menus.md`; slice design §7.6–7.10.
- Tester: Claude (remote session), Linux cloud workspace, GCC 13 Debug; SwiftShader under `xvfb-run`.
- State: **implemented/unverified** on Windows.

## 1. What changed

- **The save** (`SaveCampaign`/`LoadCampaign`, version 2): written whole through a temp file and a rename;
  another version refused; a bad file reported and kept.
- **Progress:** `WinStage` (100 XP a level, a skirmish's XP stops at the next story stage's level, finds to
  storage, take-once items, cleared rooms, the fight's facts, T3 unlocks the rite, a story win refills every
  cleared skirmish with its next twist), `LoseStage` (nothing kept but the rite's use); T1 now gives 100 XP.
- **The map** (`map.json`, `TheatreMap`, `MapView`, `Nearest`): tickets open in order (đã qua, tiếp theo,
  giao tranh, ?), a skirmish names its twist, a locked room says what opens it.
- **Around a stage:** `PreStageFor` (the player first and always, up to the cap; the revive warning),
  `ResultOf`, `OverFor` (the reason; T1 retry only); `PartyOf`/`ApplySave` carry levels, slots, chosen
  versions and boosts into the fight.
- **The flow** (`Flow.cpp`, `CampaignApp.md`): `LastCurtain --campaign` — title (Tiếp tục, Trò chơi mới, Cài
  đặt, Thoát), new game (name, Nữ/Nam) → the prologue → T1 → the café ⇄ the map → pre-stage → the fight →
  result or game over; screens `title`, `newgame`, `settings`, `worldmap`, `prestage`, `result`, `gameover`;
  two new Play entries in `projects/reference/project.json`.

## 2. Checks (Linux)

- `UmbraCampaignTests` 39 cases: the save (round trip, version, corrupt kept, interrupted write), progress
  (XP and the cap, refills, the rite, a loss), the map (marks in order, the cursor reaches every ticket,
  panels), around a stage (party, revive warning, result, game over and the rite rules), the save into a
  fight (versions, boosts), a scripted walk through slice 1 (T1, T3, a skirmish, Stage 1, the café brings Tí
  in, the refill, the save written and read back).
- `Umbra.Tool.CafePreview`: 10 screens with `cafe-campaign-sample.json`, 0 errors, 0 warnings; every letter
  of these screens' words is in the fonts.
- Full run: see §2a. Picture: `captures/2026-11-24-campaign-title.png`.

### 2a. Full run

`cmake --build build-gcc` (BUILD_RC=0, no warnings), then `ctest -E Balance -j4`: **60/60 passed**. Stage data changed only in T1's XP (not used by the soak).

**Not run:** Clang and ASan; MSVC; playing the flow through in a window (each screen and the fight are
reached by `RunScreen`/`RunStage`; the walk is tested headless with stand-in fights).

## 3. Limits

- Game over offers Thử lại / Về bản đồ; the rite is the rules' Rite inside the fight (offered when the player
  falls, once a game). The fight's pause "Save and quit" leaves the game (a "Bỏ cuộc" to the map comes later).
- Settings hold large text, the old TV screen and the worn tape; volume, text speed, enemy speed, language and
  full screen are not there yet. The pause menu's words are still English.
- The floor plan picture is not painted (the map is drawn on dark); new-room fade and skirmish pulse are not
  animated. The café is the screen hub (plan 6).

## 4. Open: the user's checks

1. `build game --campaign` (or the editor's Play: "campaign"): a new game with a Vietnamese name, the prologue,
   T1, the café (talks), the map, T3, a skirmish, Stage 1 (free Tí), the café (Tí joins, brew Mứt gừng, give
   the note), then quit and Tiếp tục: everything is still there.
2. Lose Stage 1 once (let the Rat fall with Tí strung): game over says why; Thử lại starts it again; nothing
   from the lost try is kept.
3. After the cap, a skirmish gives no XP; after a story win its twist changes.

Result: **not run**.
