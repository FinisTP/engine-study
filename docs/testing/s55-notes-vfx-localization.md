# The Last Curtain: the user's 22 notes (16:57), the effects library, localization and the darker text — verification record

## Record

- Date: 2026-11-24.
- Requests:
  - the user's 22 notes of 16:57;
  - the effects design (`vfx-design-2026-11-24.md`, 17:21);
  - the 19:05 points: guided tutorial turns, ability sequences, horror that blocks nothing;
  - the 19:40 points: darker text, and one localization table.
- Rulings the user answered:
  - the whole tutorial ladder now;
  - skills cost HP;
  - horror shader plus effects;
  - painted floor tiles;
  - finish the notes before the effects;
  - paint the textures in parallel;
  - one effects library;
  - each lesson's first use guided;
  - sequences now, always played in full;
  - all four horror touches;
  - all short text in the CSV;
  - the text may be explicit (images keep the no-gore rule);
  - rewrite everything now;
  - effects, then localization, then text.
- Commits: `issue #UMBRA-423` to `#UMBRA-435`, records `#UMBRA-436`.
- Tester: Claude (remote session), Linux cloud workspace, GCC 13 Debug; SwiftShader under `xvfb-run`.
  Art was painted on the user's PC (ComfyUI Desktop, SDXL base); particle sheets were cut and drawn in the
  cloud.
- State: **implemented/unverified** on Windows.

## 1. What was done

| Commit | What |
|---|---|
| 423–426 | Notes 1–8, 14–17: a scene has the screen to itself; a fall fades once told; enemies never revive; the third rat comes at round 3; Băng bó heals a chosen ally; tips sit above their panels; walks go around foes; a marked square says what it is; the breath row lights once told; the turn order is a rotating queue; the camera goes from the one acting to the one reached; numbers rise over who was hit |
| 427 | Note 13: Rạch, Ná cao su and Dao mổ cost 1 HP; a role with no free strike starts with Đánh thường; t3 and the skirmishes retuned to their bands |
| 428 | Note 20: painted floor tiles on the board, one per kind of square on a board; bandage and programme icons |
| 429 | Notes 9–12, 22: the tutorial ladder T1 → T2 (hurt start, Mai heals, two on one square) → T2s (Bảo, skills cost HP) → T3 → Stage 1; XP in the notebook and on the result; bag labels wrap; café door, counter and notebook by click |
| 430 | Engine particles: a picture sheet, start angle and spin, a stretch along the velocity, a line emitter; drawn in runs by picture (Vulkan and the browser) |
| 431 | Sequences (`Sequences.md`): timed cues per told line (camera, effect, flying effect, hit, shake, sound, bubble); the effect library and its 11 sheets; ghosts' dread; the Giảm setting |
| 432 | Horror that blocks nothing: cold, flickering ghosts; a tracking tear on the 3D view when an enemy acts or a heavy blow lands (`Post.md` 6b); the lamps stutter; dust in the light; a waiting portrait twitches |
| 433 | `localization.csv` (key, vi, en) replaces `match-words*.json` and `campaign-words.json`; a guard against text typed into the campaign's screens |
| 434 | Every line darker, Vietnamese and English: scene lines carry `en`; stage titles and aims, map names, cast names, skill lore, drinks and memories moved into the table |
| 435 | Guided tutorial steps: one lit square, one action, one target, one battle pick, and a tip; ending the turn stays possible when the action can't be taken |

## 2. Checks (Linux)

- `ctest -E Balance -j3`: **60/60** (§2a).
- Balance after the HP costs:
  - `UmbraCampaignTests "[campaign-balance]"`: every band holds.
  - New bands: T2 and T2s 85–100%, the same as T1.
  - The skirmishes sit near their band edges: sk_boxoffice 70%, sk_props refill 1 70.5%, sk_stairs refill 2 82%.
  - `UmbraMatchTests "[match-balance]"`: passes (24 min run alone).
  - `Umbra.Night.Balance`: passes.
- New tests (RED first where a function was missing):
  - sequences load, play in time, and defer the hit;
  - skipping spawns no effects and agrees with a fight that has no sequences;
  - particle pictures: angle and spin, cells, stretch, the line emitter, runs by picture;
  - vfx fields are parsed and their mistakes named;
  - the tear is packed;
  - unease;
  - the Giảm setting saves;
  - localization: nested words and lists, both languages everywhere, no typed text in the campaign screens;
  - every scene line has Vietnamese and English;
  - every stage, ticket, cast member and recipe has its text in the table;
  - guided steps: one square, one action, its target, let go once done;
  - the T1 guide resolves to the square before the rat;
  - start HP, XP gained, café spots by click, enemies never revive, battle heal, walks around foes.
- Pictures (SwiftShader):
  - T1's first turn: the guided Move row lit, the tip at the bottom, and a pale ghost.
  - The painted floors.
  - The café spot labels.
  - The notebook XP bar and wrapped bag labels.
  - The particle sheets on a dark ground.

### 2a. The last full run

`ctest -E Balance -j3` on `#UMBRA-435`: **60/60 passed** (325 s). `Umbra.Night`'s
"same seeds after Continue" test (a worker thread with 2 ms sleeps) failed once under `-j6` and passed twice
alone; it is an existing timing-sensitive test, not something this work changed.

**Not run:**
- MSVC, Clang, ASan.
- Windows play.
- How the effects look up close: the stage camera stays wide in a capture.
- Anything by hand.
- The browser build. It ignores the tear and was not built this session.

## 3. Limits

- Particle textures: SDXL ignored "black background" and drew lanterns, spheres and stars. Ink, dust,
  smoke and threads are cut from the paintings by their distance from the grey ground. Glows, flecks,
  embers, streaks, beads and ash are drawn by `tools/art/vfx_sheets.py`.
- The faction match's English mode reads `match.*` English; the campaign follows the settings switch.
  Data text in English takes effect on the next load (a screen opened after the switch).
- All new and rewritten text is a draft for the user's review. The tutorial instructions inside the darker
  text stay plain.
- The guided steps restrict the player's own units. Enemy turns are scripted only where a step names an
  enemy (none do yet): the rules already make T1–T3 deterministic (no dice in these abilities).

## 4. Windows check (open)

1. `play-campaign.bat`, then a new game.
   - T1: the Move row is lit and alone, and the tip says where to go. Only the square before the rat lights;
     walk, Attack, then only Đánh thường in the battle.
   - Effects: an ink dab and a jolt on the hit; the rat pale, flickering, with smoke at its feet.
   - Win: "+100 XP" on the result.
2. Café.
   - Click "Ra cửa" or "Pha chế" over the room.
   - The notebook: an XP bar; "Tờ chương trình cũ" wraps inside its slot.
3. T2.
   - You start at 9 HP.
   - Mai's turn: only Kỹ năng → Chăm sóc → you.
   - Your turn: only Mai's square, then end the turn.
4. T2s: Bảo is guided beside a ghost, then Dao mổ costs him 1 HP; the scalpel streak shows.
5. T3: when the row turns red, only the squares off it light.
6. Settings → "Hiệu ứng hình ảnh: Giảm": fewer particles, no shake, softer tear.
7. Language → English: menus, the map, stage titles and dialogue all in English, with no Vietnamese left;
   then back.
8. Read the darker lines (prologue, T1–T3, Stage 1, the café talks, Tí's memories) and mark what to change.
