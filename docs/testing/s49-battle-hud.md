# The Last Curtain, campaign plan 5: the battle HUD, words and large text — verification record

## Record

- Date: 2026-11-24 (overnight, unattended).
- Commits: `issue #UMBRA-365`, `#UMBRA-366`, `#UMBRA-368` to `#UMBRA-373` (`#UMBRA-367` is plan 4's records),
  records `#UMBRA-374`. Plan: `docs/design/plans/2026-11-23-campaign-plan-5-battle-hud.md`; spec:
  `docs/design/ux/hud.md`, `ux/interaction-patterns.md`, `ux/fight-screen.md`.
- Tester: Claude (remote session), Linux cloud workspace, GCC 13 Debug; SwiftShader under `xvfb-run`.
- State: **implemented/unverified** on Windows; the Vietnamese fight words are a **draft**.

## 1. What changed

- **Words:** every label of the fight's screens comes from `match-words.json` (`screens`); a full Vietnamese
  copy `match-words.vi.json` (same keys, test-checked) is used by `--stage`; the career keeps English.
- **Large text:** `UiSystem::SetTextScale` (engine) rebuilds the fonts ×1.25; a "Large text" switch in the
  pause menu, kept with the CRT and tape switches in the player's `screen.json`.
- **Keys:** `assets/keys-keyboard.json` (back, confirm, next, hurry, endTurn, skip, log, pin, notebook,
  menu1–6); `InputKey` gains 5 and 6 (appended, older values unchanged).
- **The campaign HUD** (`ui/campaign-hud.ui.json`, only in a stage): the goals card (ticks drawn as boxes,
  the trapped ×), this round's strip with cast names, the hint and the last log line, toasts top right, the
  unit card (I pins it).
- **The command menu** beside the acting unit: Đứng yên / Kết thúc lượt while walking; Di chuyển, Tấn công,
  Kỹ năng », Vật phẩm », Cắt dây (only beside a fallen strung one), Kết thúc lượt; number keys; targets as
  numbered rows with the preview ("5–7 · trả 3–4", "nhanh hơn: ×2"); Back (Esc, right-click): targets →
  sub-menu → the walk → pause.
- **On the board:** cast names on the plates, ghosts at 0.8 height, the fallen at half opacity, a red string
  over each strung unit, telegraph squares a round ahead (hazard amber, breath crimson, window violet, arrival
  pale). Their turn: "Lượt của {name} · Giữ Space nhanh hơn · F bỏ qua"; Space held hurries ×3, F skips;
  J (notebook) pauses.

## 2. Checks (Linux)

- `UmbraMatchTests "[play]"`: 31 cases (words in screens and both words files; HUD goals, strip, hint; goals
  ticking and failing; the menu by situation, number keys, sub-menus and the back stack; armed target rows;
  preview equal to the rules' `Damage`; the card and pin; telegraphs; toasts).
- `UmbraUiTests`: the text scale (larger, back to 1, refused out of range). `UmbraNightTests`: the key file
  (defaults, a remap, unknown rows refused), the large-text switch saved, every letter of the campaign's
  Vietnamese words and screens in the body, small and strong fonts.
- Full run: see §2a.
- Pictures (SwiftShader, analog screen on): `captures/2026-11-24-stage1-large-text-vi.png`,
  `captures/2026-11-24-campaign-hud.png`, `captures/2026-11-24-command-menu.png`,
  `captures/2026-11-24-cast-on-board.png`.

### 2a. Full run

`cmake --build build-gcc` (BUILD_RC=0, no warnings), then `ctest -E Balance -j4`: **59/59 passed**. The soaks are unchanged by this plan (no rule or data change); they passed on plan 4's last data (`s48` §2a).

**Not run:** Clang and ASan; MSVC; mouse-only and keyboard-only play in a window (the menu's key and click
paths are tested headless; walking by keys waits for plan 8's board cursor).

## 3. Limits

- The figures are still the faction match's (cast sprites and tints, the gold knot, the freed unit's snap and
  lift come with the art pass).
- The shaded HP segment on the target's plate is not drawn yet; the preview is in the target rows and the card.
- The pause menu and the battle view's own screen (`match-battle`) still use the old layout (pause words are
  English until plan 7's menus).
- Plain colours; the approved style A art is not wired in yet.

## 4. Open: the user's checks

1. `build stage stage1`: play with the **mouse only** (click squares, the menu rows, targets; right-click
   steps back), then with the **keyboard only** after Đứng yên (1–6, F, Esc, I, Space held on their turn).
2. Pause → "Large text: on": every panel grows, nothing overlaps at 1280×720; it stays on after a restart.
3. Read `match-words.vi.json` and the menu words; are "Đứng yên", "Kỹ năng", "Vật phẩm", "trả" right?
4. The analog screen on and off: is the HUD readable through it?

Result: **not run**.
