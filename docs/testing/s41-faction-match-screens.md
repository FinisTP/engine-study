# The Last Curtain: faction matches, Plans 2 and 3 (the centre's monsters, the screens, the polish) — verification record

## Record

- Date: 2026-11-20 (overnight, the user away most of it)
- Branch: `factions`. Commits `issue #UMBRA-264` to `#UMBRA-288` (Plan 1's saves were `#UMBRA-263`).
- Tester: Claude (remote session), Linux cloud workspace; pictures under SwiftShader (Chromium's
  Vulkan ICD) with `xvfb-run`; art made in the user's ComfyUI on their PC.
- State: **implemented/unverified** for play on Windows. Rules, bots and the match's screen logic
  (`MatchPlay`) are tested headless; the screens and the table are checked by picture on Linux only.
- Design: `docs/design/2026-11-19-factions.md` (updated 2026-11-20); plans:
  `docs/design/plans/2026-11-19-factions-plan-2-screens.md`, `docs/design/plans/2026-11-20-factions-plan-3-polish.md`.
  The approved layout is the prototype at https://claude.ai/artifact/4LB1dit1jm2niE3gz8uFLw.

## What changed

| Commit | Change | Tests |
|---|---|---|
| `#UMBRA-264` | Data: spawns.csv, monster level/size/sight, ability targets | T310, T311 |
| `#UMBRA-265` | A fixed stage; monsters spawn instead of returning; saves version 2 | T312-T314 |
| `#UMBRA-266` | Monsters of 2x2 and 3x3 squares | T315, T316 |
| `#UMBRA-267` | Hunters start battles in a resumable monsters' phase; `MonsterPick` | T317-T319 |
| `#UMBRA-268` | Bots mind hunters; the user's rebalance (the centre spawns once; den monsters belong to their half's side) | T320, T321, T340, T341 |
| `#UMBRA-269`-`271` | `MatchPlay`: beats, banner, order, log; the turn (reach, actions with reasons, targets, forecast); battle, level, event, end | T322-T330 |
| `#UMBRA-272` | The screen files; the engine UI's `tip` (hover reasons, disabled widgets too) | T331, UI "A widget's tip..." |
| `#UMBRA-273` | `MatchTable` and `MatchApp`: the match runs in `LastCurtain` (`--night` keeps the night); laid out as the prototype after the user's feedback | pictures |
| `#UMBRA-276` | The engine UI's `fit` (contain, cover, top): portraits keep their shape | UI "A picture keeps its shape..." |
| `#UMBRA-277` | Shadows, a cold horror grade that deepens (no red), quiet marks for optional rooms | pictures |
| `#UMBRA-278` | Motions and sounds from each told line | T342 |
| `#UMBRA-279` | Camera follow, the turn ring, opening and closing scenes | pictures |
| `#UMBRA-280` | Plan 3 | — |
| `#UMBRA-281` | Spoken lines in bubbles; twins told apart ("(theirs)") | T343 |
| `#UMBRA-282` | Painted rooms; a seamless zoom into them (walls rise); a bot's level choice no longer opens the player's screen | T344, pictures |
| `#UMBRA-283` | ComfyUI art: the pick backdrop, the turn banner, the fighter cards | pictures |
| `#UMBRA-284` | Treasure: 2-5 face-down cards | T345, T346 |
| `#UMBRA-285` | Claimed rooms come back in the same half | T347 |
| `#UMBRA-286` | ComfyUI icons: HUD numbers, "?" and chest marks | pictures |
| `#UMBRA-287` | Level-up cards: learn, a stat, an upgrade | T292 (rewritten), T348 |
| `#UMBRA-288` | The event's dice | T349 |

## 1. Automated checks

```bash
cmake --build /home/claude/build-gcc --target UmbraMatchTests UmbraUiTests UmbraNightTests
/home/claude/build-gcc/plugins/last-curtain/tests/UmbraMatchTests "~[match-balance]"
/home/claude/build-gcc/plugins/last-curtain/tests/UmbraMatchTests "[match-balance]"
bash /home/claude/build.sh clang; (cd build-clang && ctest -R "Umbra.Match$|Umbra.Ui$|Umbra.Night$")
bash /home/claude/build.sh asan;  (cd build-asan && ctest -R "Umbra.Match$|Umbra.Ui$")
```

| Toolchain | Result |
|---|---|
| GCC 13, Debug | UmbraMatchTests 64/64 test cases (balance excluded); balance passes; UmbraUiTests 11/11; UmbraNightTests pass (balance excluded) |
| Clang, Debug | Builds; Umbra.Ui, Umbra.Night, Umbra.Match pass (3/3) |
| GCC ASan+UBSan | Builds; Umbra.Ui, Umbra.Match pass (2/2), no sanitizer reports |

## 2. Balance (400 matches, medium against medium)

**Holds at `#UMBRA-287`**: side 0 48.0%, first mover 50.5%, monsters 43.8% / kills 43.3% /
guardians 12.9%, comebacks 27.4%. History: the centre's hunters first broke it (monsters 78%, comebacks
15%) and data could not fix it; the user's rule (`#UMBRA-268`: the centre spawns once, dens belong to
their half's side) restored it; treasure and returning rooms moved comebacks to about 24%, restored by
data in `#UMBRA-287` (`Bots.md`).

## 3. Pictures (Linux, SwiftShader)

`LastCurtain --watch --seed 5 --frames 240 --capture match.png` (the board), `--zoom 12` (the room
view), the pick screen, and `UmbraTool ui-preview match-hud <screen> --data assets/ui/match-sample.json`
for every screen. These show layout, not the look on a real graphics card.

## 4. Open: the user's Windows checks

Build and run (`cmake --build --preset windows-msvc-debug`, then `out/build/windows-msvc/.../LastCurtain.exe`):

1. The pick screen: the opera backdrop; pick three; bot level; Start lights only with three; Continue
   after quitting mid-match.
2. The opening scene sweeps onto the board; the turn banner (brass) with "Turn N" and turns left.
3. Your turn: the lit reach; the path follows the mouse; click a square to walk, yourself to stay;
   the action list; hover a disabled action: its reason shows.
4. Attack and a board ability: targets red, the range orange, the forecast follows the mouse; the
   battle screen with the framed fighters; picks with damage and cost.
5. A hunter in the centre attacks a character of yours: the battle waits for your picks.
6. Treasure: walk onto a chest, Pick up, choose a card, see what it was. Level-up: three cards.
   An event: choose, the dice tumble and land, the result.
7. The board: figures move (attack, hit, fall), sounds play, the camera follows, the gold ring opens
   each turn, bubbles show short lines, shadows under figures, the grade grows colder.
8. The wheel zooms smoothly from the board into the rooms (walls rise) and back; a drag pans.
9. The end screen and the closing scene; Play again.
10. Frame rate and look on your card; anything that reads badly. `LastCurtain --night` still runs the night.

## 5. Known limits

- The monsters' phase is resolved at once; the screen tells it one log line per beat, so figures jump
  square to square while the lines pace.
- Spoken lines are written lines; the built-in model does not write them yet.
- A downed figure leaves the board at once (its fall motion is barely seen).
- The room view is the board's own space with walls raised, not the night's separate room stage.
- The ComfyUI panels and bubbles were turned down (busy or unclean); the bubble is a plain style.
