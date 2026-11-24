# The Last Curtain, the user's 2026-11-24 requests and campaign plan 8 (input) — verification record

## Record

- Date: 2026-11-24 (the user's message of 09:11: the café as a room, a basic attack for everyone, the UI
  cutouts wired, a retreat button, the floor plan painted, the remaining spec).
- Commits: `issue #UMBRA-389` to `#UMBRA-399`, records `#UMBRA-400`.
- Spec: `docs/design/ux/interaction-patterns.md` (P1, P4, P15, P17), `ux/menus.md` (C, D), `ux/world-map.md` §6,
  `docs/design/ui-art-requests-2026-11-23.md` §1.
- Tester: Claude (remote session), Linux cloud workspace, GCC 13 Debug; SwiftShader under `xvfb-run`.
- State: **implemented/unverified** on Windows.

## 1. What changed

- **Basic attack (#389).** `basic` (Đánh thường) is every character's first ability; Mai's kit moved down one;
  the Rat −1 attack and the box office windows retuned so the soak bands hold.
- **The floor plan (#390).** Painted on the PC (SDXL, `tools/art/ui_floorplan.json`), composed with
  `tools/art/floor_plan_compose.py`: rooms traced from `map.json`, each ticket on its room.
- **Retreat (#391).** Rút lui in the fight's goals card (and the pause) asks Không/Có; Có goes back to the map
  with nothing from the stage kept.
- **The café as a room (#392).** `CampaignRoom` + `cafe-room.json`: the player and allies walk (WASD, arrows,
  a click, the stick), Tab / Y hands the walk on, E / Space / A talks, brews at the counter, opens the notebook
  or leaves by the door; names float over heads, gold for a new talk.
- **Engine fix (#393).** `LoadImageFile` refuses a folder instead of throwing (a screen's empty image path
  crashed the preview).
- **UI art, slice 1 (#394).** `art/ui/campaign/` (lacquer frames, icons, ingredients; `art/README.md`
  "Campaign UI"), the skin's `lc*` styles on every campaign screen; mark icons on the map's tickets, drink and
  ingredient icons in brewing, command icons in the fight's menu.
- **Settings and pause (#395).** Settings has every row of menus.md D: full screen, text size, the two TV
  effects, effects volume, text speed (Chậm, Vừa, Nhanh, hiện ngay), enemy turn speed, the fight's language,
  and the keys. A stage's pause is Vietnamese: Tiếp tục, Cài đặt, Rút lui. Settings opens from the title,
  the map, the café and the fight's pause.
- **The cast on the board (#396).** A stage's figures are the cast's sprites (the player's by gender; a
  strung recruit's bound sprite until cut).
- **The map's news (#397).** A toast for each room opened since the map was last shown and one for new
  skirmishes; the new room is focused.
- **Gamepad and keys (#398).** A pad button beside each key meaning (`keys-keyboard.json` "pad"), d-pad / A / B
  in every menu, Menu pauses; the board cursor (WASD, the stick; Enter / Space / A acts on its square).
- **On-screen keyboard (#399).** Bàn phím on New game: Vietnamese letters, the tone mark on the right vowel
  (`Typing`, `typing.json`), capitals at a word's start.

## 2. Checks (Linux)

- New tests: the café room (walk, talk, brew, notebook, door, switch), a folder as an image, settings (each
  action, limits, read back), pad buttons (defaults, either presses, the file, a bad name), the board cursor
  (start, step, edges, repeat), the tone mark (Lan, Mai, Quy, Gia, Toàn, Hường, replace, remove, no vowel,
  capitals, a letter's backspace).
- `Umbra.Tool.CafePreview`: 12 screens (keyboard and the stage's pause added), 0 errors, 0 warnings.
- Full run after #399: `ctest -E Balance -j4` **60/60**; `Umbra.Campaign.Balance` passes.
- Pictures: `captures/2026-11-24-campaign-cafe-room.png`, `…-campaign-map-lacquer.png`, `…-campaign-settings.png`.

**Not run:** Clang, ASan, MSVC; anything with a real gamepad; full screen on a real display; sound volume
by ear.

## 3. Limits

- The board cursor uses WASD and the stick; arrows and the d-pad stay with the command menu. Enter / A act
  on the cursor only after it was moved (the mouse takes over when it moves). Q / E target cycling (P4) and the
  path drawing are not there yet.
- Prompts do not yet switch to gamepad glyphs; the glyph art (§1.5) is not made.
- Music volume is not in Settings (the game has no music yet). The language switch changes the fight's words
  only; scenes and the café are Vietnamese.
- The map's new-room fade and skirmish pulse are toasts and focus, not animated.

## 4. Open: the user's checks

1. The café: walk, switch with Tab, talk to Lệ Thanh, brew at the counter, the notebook by the board, leave by
   the door. With a gamepad: the stick, A, Y.
2. A fight: the cast's figures; the command icons; Đánh thường; WASD/stick cursor then Enter/A; Rút lui → Có.
3. Settings: full screen on and off, effects volume, text speed in a scene, enemy speed, English in a fight.
4. New game with the gamepad: Bàn phím, type "Hường" (h, ư, ơ, n, g, Huyền).
5. The look of the lacquer screens and the painted map.

Result: **not run**.
