# The Last Curtain, campaign plan 3: dialog scenes — verification record

## Record

- Date: 2026-11-23.
- Commits: `issue #UMBRA-341` to `#UMBRA-346`. Plan: `docs/design/plans/2026-11-23-campaign-plan-3-dialog-scenes.md`;
  spec: `docs/design/2026-11-23-campaign-slice-1.md` §7.11.
- Tester: Claude (remote session), Linux cloud workspace, GCC 13 Debug; SwiftShader (Subzero) under `xvfb-run`.
- State: **implemented/unverified** on Windows; the prologue's Vietnamese is a **draft** for the user's review.

## 1. What changed

- **Engine UI** (`engine/ui/include/UI.md` "Pictures"): an `image` takes `flip` (mirror left to right)
  and `tint` (a colour multiplied in); a tint that is not a colour is refused, naming the field.
- **`modules/campaign`** (new, headless; `Campaign.md`): `cast.csv` (`id,name,sprite,bound,form,voice`;
  a `nameless` row gives the default player name); scene files `assets/campaign/scenes/<id>.json`
  validated (unknown cast or face, a third portrait on a side, a speaker not on stage, one choice or more
  than four, a jump to no id, an id twice, a loop with no choice); `ScenePlayer` (typewriter by letters,
  Advance, Skip that stops at a choice, Choose sets a fact, portraits with the speaker lit, the log,
  the line's sound); `FillTokens` (`{player}`, `{anh/chị}`, `{Anh/Chị}`).
- **The app:** `ui/dialog.ui.json` (backdrop, two portraits a side facing the middle, the name tag on
  the speaker's side, the box, », choices above the box, Nhật ký and Bỏ qua), `ui/dialog-log.ui.json`,
  `campaign-words.json` (vi, en), `DialogView` (`Umbra::CampaignPlay`: faces fall back to calm, then to
  none; tints by form, dimmed when not speaking), `DialogApp` and `LastCurtain --scene <id> [--backdrop
  <name>] [--name <text>] [--gender male|female]`; `options.csv` `lettersPerSecond` 40.
- **The prologue** (`scenes/prologue.json`, 15 lines): the night tour, the torch dies, Lệ Thanh on the
  stairs knows the player's name, the full house of empty seats, a choice (`lethanh.first` kind or wary),
  a red string creaks down.

## 2. Checks (Linux)

- `Umbra.Ui`: "An image can be mirrored and tinted" (u = 1 when mirrored, half colour, `root.children[1].tint` named).
- `Umbra.Campaign` (new): 10 test cases, 200 assertions — the cast and a bad form; a scene loads; one
  broken case per rule, each naming file, line and fix; the typewriter never splits a letter; portraits
  enter, leave and light; Skip stops at a choice (also on the last line); Choose and facts; the log;
  tokens for both genders and the nameless default; `DialogData` (flips, tints, the fallback face, the
  name's side, choices, the log); the prologue played to its end both ways.
- `Umbra.Tool.DialogPreview` (new CTest): `UmbraTool ui-preview dialog dialog-log` with
  `ui/dialog-sample.json`: 0 errors, 0 warnings.
- Picture: `captures/2026-11-23-prologue-dialog.png` — `LastCurtain --scene prologue --backdrop
  foyer_today --name Lan --gender female --capture ... --frames 60` under SwiftShader with the analog
  screen on: the player left facing right (dimmed), Lệ Thanh right, mirrored to face her, lit, name tag
  on her side; the choice line; Vietnamese readable through the CRT and tape.
- Full run: `cmake --build build-gcc`, then `ctest -E Umbra.Match.Balance`: see §2a.

### 2a. Full run

`cmake --build build-gcc` (BUILD_RC=0), then `ctest -E Umbra.Match.Balance`: **60/60 passed** (61 tests now:
the two new ones are `Umbra.Campaign` and `Umbra.Tool.DialogPreview`; the match soak, unchanged by this
plan, passed earlier the same day in `s46`).

**Not run:** Clang and ASan; MSVC; the in-game click path (the capture skips to the choice; clicks and
keys are wired in `DialogApp.cpp` but not driven by a test).

## 3. Limits

- The scene is played from a developer flag only; the campaign's flow (title, map, café) is plan 5.
- Facts chosen are not saved yet (plan 5's save).
- The player's portrait is the shared `player_female` / `player_male` sprite.
- Tí's strung sprite has only a calm face, so every strung line shows it calm (the fallback).

## 4. Open: the user's checks

1. **Read the prologue** (`plugins/last-curtain/assets/campaign/scenes/prologue.json`) as a native
   speaker: is it natural, is the hook strong, is "Nhà hát Thành phố" the name you want, are the two
   choices worded right? Change freely; the test only needs the fact `lethanh.first` = `kind` / `wary`.
2. On Windows: `LastCurtain.exe --scene prologue --backdrop foyer_today --name <your name> --gender
   <male|female>`: the letters appear one by one; a click or Space completes the line, the next goes
   on; 1 or 2 (or a click) picks a choice; Bỏ qua (or Esc) runs to the choice and stops there; Nhật ký
   shows the lines so far and Đóng closes it; the window closes at the end.
3. Is the dimming of the one not speaking clear enough? Is 40 letters a second right
   (`options.csv` `lettersPerSecond`)?

Result: **not run**.
