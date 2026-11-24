# The Last Curtain, the user's 19 notes of 2026-11-24 15:10 — verification record

## Record

- Date: 2026-11-24 (the user's 19 notes after the second play; rulings asked and answered: moves of 2–4
  squares; pick then place in the notebook; locked skills greyed with their unlock level; a new male
  portrait made on the PC).
- Commits: `issue #UMBRA-414` to `#UMBRA-421`, records `#UMBRA-422`.
- Tester: Claude (remote session), Linux cloud workspace, GCC 13 Debug; SwiftShader under `xvfb-run`;
  art made on the user's PC (ComfyUI Desktop, SDXL base, ExpressionEditor).
- State: **implemented/unverified** on Windows.

## 1. The notes and what was done

| # | Note | Done | Commit |
|---|---|---|---|
| 1 | dialogue: camera not on the speaker | the close-up now also moves the room view's seat (a stage is in the room view); a narrated line keeps the last speaker in view | 414 |
| 2 | menu covers the actor; squares lit before Di chuyển | no squares (and no walking by click) until Di chuyển; the menu stands clear right of the figure (left near the screen's edge), in the UI's scale | 414 |
| 3 | moves cover the whole board | Đi 2–4: attackers, all-rounders, speedsters 4; defenders, supporters 3; Tí's +2 dropped; the stairs and box office retuned | 415 |
| 4 | dialogue over the opening shot | nothing plays, no scene, no screen or HUD until the opening shot ends | 414 |
| 5 | things happen at once | opening shot → "Trận đấu bắt đầu" banner → first turn; the end: last blow → the fight's view leaves → "Chiến thắng" / "Thất bại" → the end scene | 414 |
| 6 | "Bóng chuột 5-5 trả 1-1"; target from a panel | no panel while aiming: red squares on the board, a card at the hovered target ("Gây 5 · bị trả 1") | 414 |
| 7 | menu before the walk ends; undo walks back | the menu waits until the figure arrives; undo jumps back | 414 |
| 8 | top UI covers heads in the battle view | the battle shot and the room view look higher (`match-look.json` battle.lift 1.1, rooms.lift 1.4) | 414 |
| 9 | "4-4", "cost 0 HP" | one number (a range only when the roll varies), an icon, a red "-1 HP" only when it costs; skill rows the same | 414 |
| 10 | camera work per move | a short push-in and turn per motion (`match-look.json` "moves": attack, power, down) | 414 |
| 11 | dialogue before the fight is over | the outcome banner holds the end scene back; a stage has no end screen ("Thoát") any more | 414 |
| 12 | hover through a transparent top UI | engine: the pointer belongs to the top screen under it (its painted boxes and choices, or all of it when dim/modal); clicks stop there too | 416 |
| 13 | "giấy chứng tử - tầm 0 ô - sức 0 - tốn 0 hp"; locked skills | each skill has a line in the character's story (`skills.json`, drafts) and its numbers only when they say something; locked ones greyed with "Mở ở cấp n" | 419, 420 |
| 14 | hovering a skill shows nothing | skill and item rows have their story and what they do as the tip; battle picks too | 414 |
| 15 | café talk: someone in front hides the speaker | figures between the camera and the speaker fade (the sprite shader's dither) | 421 |
| 16 | items as a toggle; storage unseen | storage listed with counts; pick an item → its card → Mang theo; pick a slot → Bỏ ra; an empty slot picked first is where it goes | 420 |
| 17 | four skills; tutorial leaves some out; skills beside | four slots; Bảo starts with Đánh thường and Dao mổ, Mai with Đánh thường, Chăm sóc, Cơm nguội; the rest in stock on the right; fights carry the loadout | 419, 420 |
| 18 | Bé Tí as "?" | people not met are left out | 420 |
| 19 | settings over an empty sky | the place it was opened from stays behind, dimmed (settings, result, game over) | 417 |

Also: Vietnamese ability names in the fight (image 2 was English: the fight loaded its own rules); the
turn's first hint ("Lượt của bạn: Di chuyển để đi…"); the male player's eyes open (#418).

## 2. Checks (Linux)

- New tests: no lit squares or walking before Di chuyển, undo snaps; targets on the board, no panel; the
  start banner and the outcome before anything else (and a plain match keeps its end screen); picks as one
  number with a red cost only when it costs and a tip; skill rows' notes and tips; the room view's seat
  nearer and higher, a shot's lift; the pointer kept by the top screen (hover and click); skills (unlock,
  starts, four at most, Equip/Unequip, the fight carries the loadout); the notebook's pick then place;
  unmet people left out; who hides the speaker. The menu, flow, picks, rows and pointer tests were run
  against the old code and failed; the camera, skills, notebook and café-room tests call functions that did not
  exist yet, so they failed to compile.
- Balance after the moves: every band in `balance.json` holds (`Umbra.Campaign.Balance`), after easing
  the stairs (a ghost 2→1, another 2→3) and the box office (an attacker 4→3).
- `ctest -E Balance -j4`: **60/60**.
- Pictures: `captures/2026-11-24-t1-opening-shot.png` (no UI during the shot), `…-t1-first-turn.png`,
  `…-notebook-pick-place.png` (preview), `…-male-eyes-picks.png`.

**Not run:** MSVC, Clang, ASan; Windows play; anything by hand with a mouse (hover, the aim card, the
menu's place at 1080p, the camera kicks, the faded figure, settings over the café).

## 3. Limits

- The first frame after a screen opens still sees the pointer (its painted parts are last frame's).
- Bandage and programme have no icon (names only); they need painting.
- Skill stories are drafts for review; the player's class skills got short generic lines.
- The scheming face keeps a small smudge by the eye.
- A narrated line with no earlier speaker leaves the camera where it was.

## 4. Windows check (open)

1. `play-campaign.bat`, male. T1: the board sweeps in with no UI, then "Trận đấu bắt đầu", then the first
   turn. Squares light only after Di chuyển; the menu sits clear of the figure and appears after the walk;
   Hoàn tác jumps back. Attack: no panel, red squares, a card on the hovered foe. Win: the view leaves the
   fight, "Chiến thắng", then the end scene.
2. In the café: the male faces have open eyes; click Lệ Thanh; anyone in front of her fades.
3. Notebook → Bảo: Chẩn đoán in stock → Mang theo; Giấy chứng tử greyed "Mở ở cấp 3"; a bandage from Kho
   đồ → Mang theo; pick a slot → Bỏ ra. Hover over the notebook: nothing under it lights up.
4. Settings from the café or the fight's pause: the place behind, dimmed.
5. T3: Bảo fights with only what he carries.
