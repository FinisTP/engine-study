# The Last Curtain: tiles, group battles, the told match, the café hub (plan 4) — verification record

## Record

- Date: 2026-11-20/21 (overnight; the user away from 23:30)
- Branch: `factions`. Commits `issue #UMBRA-291` to `#UMBRA-297`.
- Tester: Claude (remote session), Linux cloud workspace; pictures under SwiftShader with `xvfb-run`.
- State: **implemented/unverified** on Windows. Rules, bots, `MatchPlay`, the café's rules and
  `CafePlay` are tested headless; the table, the fight stage and the café room by picture only.
- Plan: `docs/design/plans/2026-11-20-factions-plan-4-tiles-groups-cafe.md`.
- Shipping: `#291`-`#293` were sent to the PC as one patch (`Temp/ship-432217f.mbox`) when the PC
  dropped off mid-`git am`; whether it applied is unknown (expected tree `c0ee906`). From `#294`
  on, nothing is on the PC yet.

## What changed

| Commit | Change | Tests |
|---|---|---|
| `#UMBRA-291` | A heal skips anyone at full HP (`FullHp`); abilities' `reach` column: `walk` or `straight` (Manhattan, through walls) | T353, T354 |
| `#UMBRA-292` | The board is tiles (walls are tiles; doorways between squares); distances in data doubled; two of a side share a tile; group battles (everyone picks in speed order, at a foe of their choosing); saves version 3 | T355, T356; board, bot, monster and turn tests rescaled |
| `#UMBRA-293` | The match told clearly: blows one by one (the player's too), cards follow told lines, the fallen stand until told; no banner or next actor while a fight is staged; four on the stage with the picker's arrow; engine UI hover, pressed and an eased scale; pick screen highlights; reach washes; docked actions; twelve-portrait order strip with tips and click-to-look; a hint at every moment; big essence; ability descriptions on level cards | T357, UI "Buttons answer the pointer" |
| `#UMBRA-294` | The café's rules: stations, recipes, ten ghosts, a timed shift, tips, the career file | T360-T363 |
| `#UMBRA-295` | `CafePlay`: walking, spots, the day's shift and takings, talks told once, the company board; `CareerMatch` | T364-T367 |
| `#UMBRA-296` | The café on screen and the loop: the game opens in the café; the stage door to a match; back with the career | pictures |
| `#UMBRA-297` | Two new ghost kinds in the match (the Box Office Girl, the Pit Pianist); this record | soak |

## 1. Automated checks (GCC 13, Debug)

```bash
cmake --build /home/claude/build-gcc --target UmbraMatchTests UmbraCafeTests UmbraUiTests LastCurtain UmbraTool
/home/claude/build-gcc/plugins/last-curtain/tests/UmbraMatchTests "~[match-balance]"   # 76 cases pass
/home/claude/build-gcc/plugins/last-curtain/tests/UmbraCafeTests                       # 4 cases pass
/home/claude/build-gcc/tests/ui/UmbraUiTests                                           # 12 cases pass
UmbraTool match-soak --data plugins/last-curtain/assets/match --runs 400 --seed 1
```

Clang and ASan were not run this round.

## 2. Balance (400 matches, medium against medium)

| After | Side 0 | First mover | Monsters / kills / guardians | Comebacks |
|---|---|---|---|---|
| tiles and shared tiles (`#292` before group battles) | 49.2% | 48.2% | 43.2 / 51.1 / 5.6 | 31.3% |
| group battles (`#292`) | 48.4% | 52.4% | 43.8 / 50.1 / 6.1 | 30.1% |
| two new ghosts (`#297`) | 48.0% | 49.7% | 42.4 / 50.4 / 7.2 | 29.9% |

A soak now takes about 19 minutes (the tile board is four times the squares).

## 3. Pictures (Linux, SwiftShader)

The board as tiles with papered wall blocks; a fight on its own stage (backdrop by kind, the
picker's arrow); the café with the shop open: four ghosts at their tables with orders and patience,
the walker, the clock and the cup.

## 4. Open: the user's Windows checks

1. The game opens in the café. Walk (WASD, arrows, a click on the floor); E at the sign opens the
   shop. Make orders at the stations (the cup panel shows the steps and what they make), serve at
   a table, watch a ghost leave when its bar runs out, empty a wrong cup at the bucket.
2. A served ghost talks with your walker (bubbles); the notice board shows the company and "Walk as".
3. Close the shop (or let the clock run out): the takings screen; the money stays after a restart.
4. The stage door: the pick screen ("Back to the cafe" too), a match, its end ("Back to the cafe"):
   the money grows by your score, and your characters keep their levels and learned abilities
   next match.
5. In the match: walls read as walls; two of your side share a tile; attacking a tile of two
   starts a two-against-two fight; every blow is told before the next; the fallen fall when told.
6. Buttons grow on hover and dip when pressed; the pick screen's chosen cards are lit and lifted.
7. The order strip: twelve portraits, hover shows numbers, a click looks at that one.
8. The reach and targets blink as half washes; the actions panel sits beside the one acting.
9. Frame rate on your card in the café and in a full match.

## 5. Known limits

- The ten ghosts still share one picture (the PC, and with it ComfyUI, went offline before the
  art could be made). `guests.csv` names each ghost's own sprite folder, so the café picks the art
  up as soon as `art/sprites/<id>/body_calm.png` exists; `monsters.csv` names it too
  and the board falls back to the shared `specter` until a ghost's own folder exists. Prompts are in `art/README.md`.
- The café is built from boxes and the wallpaper; a painted café backdrop is planned with the art.
- Resting, shopping, unlocks and multiplayer are not built; money is kept for them.
