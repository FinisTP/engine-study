# The Last Curtain, the user's 25 notes of 2026-11-24 12:41 — verification record

## Record

- Date: 2026-11-24 (the user's 25 notes after playing the campaign build; rulings asked and answered:
  the bubble shows the ability name only; Tí's minions are "the shadow of Con Chuột"; audio is made on the PC;
  the map fills the screen and pans and zooms).
- Commits: `issue #UMBRA-401` to `#UMBRA-412`, records `#UMBRA-413`.
- Tester: Claude (remote session), Linux cloud workspace, GCC 13 Debug; SwiftShader under `xvfb-run`;
  audio and face jobs run on the user's PC (ComfyUI Desktop).
- State: **implemented/unverified** on Windows.

## 1. The notes and what was done

| # | Note | Done | Commit |
|---|---|---|---|
| 1 | typing sound in dialogue | a key click every 0.07 s while a line types | 404, 411 |
| 2 | male player's talk face (mouth open, eyes shut) | **not fixed**: every face of that sprite has the eyes shut; ExpressionEditor cannot open them (limit 5). Needs a new base portrait | 405, 408, 412 (tool) |
| 3 | dialogue in a fight: no backdrop, camera on the speaker | the board stays behind, the camera closes on the speaker | 404 |
| 4 | minions show the spider | "the shadow of Con Chuột" sprite (a darkened rat) | 402 |
| 5 | the menu covers the tiles | Di chuyển first; the menu steps aside; Back brings it | 402 |
| 6 | unbounded drag | the camera's pan is held to the board | 402 |
| 7 | random bubble lines | the ability's name | 402 |
| 8 | undo called "di chuyển" | Hoàn tác di chuyển | 402 |
| 9 | sprites off their tiles | each sprite stands on its feet (`FootCentre`) | 402 |
| 10 | "còn -1 lượt" | no count when there is no limit | 402 |
| 11 | the board spins under the turn UI at the end | no spin in a campaign; the HUD hides when the stage is over | 402 |
| 12 | text too small | UI laid out at 720 rows and scaled to the window; larger fonts | 403, 404 |
| 13 | dashed line above sprites | pictures clamp at their edges | 403 |
| 14 | café dialogue over an empty sky | the room stays behind, the camera closes on the speaker | 404 |
| 15 | music and fades; plan the sound | six places' loops with crossfades, a music volume; `docs/design/sound-design.md` | 404, 411, 413 |
| 16 | click a person to talk | yes, in the café room | 407 |
| 17 | notebook not like the mockup | three columns: people; portrait, stats, who fights, skills, bag; the skill's versions | 407 |
| 18 | "{player} chưa có chuyện gì mới" | filled everywhere | 407 |
| 19 | map: no pan/zoom, sky behind | fills the screen; drag pans, the wheel zooms; tickets stay inside | 407, 410 |
| 20 | a highlighted button grows out of view | framed buttons no longer grow on hover | 410 |
| 21 | unclear who fights | each row says Ra trận / Ở lại quán / Luôn ra trận; unpicked rows are dim; hover no longer looks like picked | 410 |
| 22 | sprites gone after retry | the fight table and café room now return their textures (the renderer holds 256; every fight and café visit leaked a full set) | 409 |
| 23 | retry should go to the roster | it does (T1, which has no roster, starts again) | 410 |
| 24 | Lệ Thanh gives nothing, no slot lesson | two bandages and a programme, then the notebook opens with the lesson | 407 |
| 25 | skill names not localized | from `campaign-words.json` "abilities" | 402 |

Also: a full font atlas now grows instead of dropping the last Vietnamese letters ("S? tay", "thư?ng"); the
title is Playfair again (#406). Double-click files for each way to play (#401).

## 2. Checks (Linux)

- New tests: the atlas keeps ổ and ỹ on every face as faces are added (failed 2/860 before the fix), bound
  sizes and clipped panels, the café gift given once and the lesson, the map lens (zoom, drag, edges), the
  notebook page, walk-to-person, the sound guard reading Ogg music (19 cues).
- `ctest -E Balance -j4`: **60/60** after #412 (Night re-run alone after the guard fix; 59/60 in the full run
  before it).
- Pictures (`UmbraTool ui-preview`): `captures/2026-11-24-campaign-notebook-mockup.png`,
  `…-campaign-map-pan-zoom.png`, `…-campaign-who-fights.png`.
- Audio: each file decodes; no gap under -45 dB for 1.5 s; loudness evened in `sounds.csv`.

**Not run:** MSVC, Clang, ASan; any listening; the retry and the café visits in the real game (the texture
cause is reasoned from the 256-texture pool and the missing frees, not watched); the crash dump
`crash-20261124-124104.dmp` in the PC's checkout was not opened.

## 3. Limits

- Note 2 stays open (see the table). Picture options: a new male portrait with open eyes, then the faces
  repainted from it.
- Music takes were kept without listening.
- Tickets near the plan's edge are pulled about 100 units inward, so at zoom 1 they sit a little off their room.

## 4. Windows check (open)

1. `play-campaign.bat`. Title: music starts. New game → prologue: a soft key under each typing line.
2. T1: the menu steps aside after Di chuyển; drag stops at the board; the bubble shows the ability's name;
   lose on purpose → Thử lại starts T1 again with every sprite in place. Do it twice.
3. Café: click Lệ Thanh → two bandages and a programme, then the notebook with the lesson. Notebook titles read
   "Sổ tay", "Đánh thường".
4. Map: drag and wheel; no sky; pick a room → who fights says Ra trận / Ở lại quán; hover a row: no growing.
5. Lose a later stage → Thử lại opens who fights; Ra trận → the board has all sprites. Repeat with two café
   visits between.
6. Settings: music and effects sliders change the level.
