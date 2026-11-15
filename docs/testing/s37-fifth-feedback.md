# The Last Curtain: the fifth round of feedback (cutscenes, turns, the hunt, sounds) — verification record

## Record

- Date: 2026-11-16
- Commits: `issue #UMBRA-214` (the engine's clickable boxes), `#UMBRA-215` (the game), `#UMBRA-216` (these records).
- Tester: Claude (remote session), Linux cloud workspace; pictures with SwiftShader; sounds made on the user's PC.
- State: **implemented**; the Windows play check (§4) is open.
- The user's feedback (2026-11-15 23:57), and answers: cutscenes **in-engine on the board**; the
  Traitor's defeat as **an HP bar and a staged final blow**; the user installed Stable Audio 3 for
  the sounds. Decided here (the user may change them): the light is switched **off** in
  options.csv for the test, as asked; the scenes' lines in `cutscenes.json` are fill-ins; the
  sounds were chosen by their shape, not by ear.

## What was built

| Items | Where |
|---|---|
| 8: the arrow and the top bar stay on the player while they rest after a room (`Acting`) | T215 |
| 5: only the right click inspects; 6: a dialog's whole box takes the click (engine: a panel or stack with an `action`, `UI.md` 5), taps count once per `tapGap` | T216, the UI test "A panel or stack with an action ..." |
| 4: `light` in options.csv (off now: everyone sees everything); 7b: a bot that walks into the dark is seen leaving, but what the room did is not told (`Beat::dark`) | T217, T218 |
| No room panel; the goal panel folds; no "In your light"; 3: the turn order, top right; 6b: portraits just over heads, the arrow above them; 7: a pulsing gold frame marks rooms to step into (the flat plate flickered) | T219 |
| 2: each turn opens with a banner, a gesture and a ring; 9: a bot's plain step holds 0.6 s instead of 2.5 s | T220, T221 |
| 8b: the Taken's HP in the top bar all hunt; a bound minion's death shows what it took from the Taken; the fall is staged in its room with who dealt it and how, then the ending | T222 |
| 1: three scenes on the board (`cutscenes.json`): the opening of a new night, midnight at the Taken, the ending; a click is the next shot, Skip or Esc ends it | T223, T224, T225 (in T222) |
| 10: nine sounds from Stable Audio 3 on the user's PC; Play names cues, `sounds.csv` maps them, the engine's mixer plays them | T226, T227 |

## 1. Automated checks

| Check | Result |
|---|---|
| T215-T227 and the UI test, each written first and seen failing (T227 with a file removed) | pass |
| Night suite: 239 cases | pass (GCC) |
| Full CTest under GCC, Clang, ASan + UBSan | 55/55 each (§1b) |

The rules did not change (the golden fingerprint holds); the light switch and `Beat::dark` only
change what is shown. No soak needed.

## 1b. Full runs

`/home/claude/verify.sh` before the sounds: GCC 55/55, Clang 55/55, ASan + UBSan 55/55, no
sanitizer report. After the sounds (a test and its link to Umbra::Audio): GCC CTest 55/55.

## 2. Pictures (SwiftShader; not a GPU check)

The opening's second shot (letterbox, the line, Skip); the board after it with the turn order, the
folded-open goal panel, labels just over the heads and gold frames on the rooms to step into;
`UmbraTool ui-preview hud` with the banner, the Taken's HP bar and the turn order.

## 4. The user's check on Windows

1. A new night: the opening plays over the board (three shots); a click moves on, Skip ends it.
2. Each turn: the banner names whose turn it is, the figure gestures, a ring grows under it; the
   turn order top right frames the acting guest. Bots' plain walks go quicker.
3. End a walk in an event room: while you rest, the arrow and the top bar stay on you.
4. Click anywhere on a dialog's box: it goes on. Click fast many times: nothing is skipped. A left
   click on a bot's turn opens nothing; a right click inspects.
5. With `light,0` you see every room and guest; set `light,1` and watch a bot walk out of your
   light: no dice or result from the dark room.
6. Midnight: its scene at the Taken, then the Turn's screen. The top bar shows the Taken's HP.
   Kill a minion: the Taken's figure shows what it lost. When the Taken dies: the fall screen in
   its room names who and how; then the ending's scene; then the end.
7. Sounds: steps, the die, doors, items, blows, falls, the midnight bell, the turn chime, clicks.
   Listen for any that sound wrong (swap the take, `assets/audio/README.md`).
