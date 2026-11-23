# Campaign plan 5: the battle HUD, words and large text — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The campaign's battle board reads at a glance and plays fully with mouse and keyboard: a
goals card, the order strip, a command menu beside the acting unit (Kỹ năng ▸ and Vật phẩm ▸ nested),
the result previewed on the target, unit cards, toasts, a hint and log line; strings, knots and falls
on the units; every word in Vietnamese from the words files; a large-text skin.

**Architecture:** `MatchPlay` gains the campaign HUD's data (`HudData`), built from `MatchState` by pure
functions in `modules/board` (`Strings`, `Plates`, `Marks`, `MenuFor`, `PreviewFor`); new screens in
`apps/LastCurtain/assets/ui/`; keys read from `keys-keyboard.json`. The career mode keeps today's
`match-hud` (the user, 2026-11-23: it stays as a side mode).

**Spec:** `docs/design/ux/hud.md`, `ux/interaction-patterns.md` (P1–P8, P10, P12, P14, P16),
`ux/fight-screen.md`; slice design §7.12; the user, 2026-11-24: **biding off** (no AP pips, no "+1 AP");
**gamepad and the board cursor come after slice 1** (plan 8), but keys already live in a key file.

## Global Constraints

- Mouse alone and keyboard alone each play everything on the board (P1). Right-click and Esc step back
  one level (P2). Number keys 1–6, F ends the turn; hold Space hurries beats; F on their turn skips.
- **Command menu:** Di chuyển, Tấn công, Kỹ năng ▸, Vật phẩm ▸, Cắt dây (only next to a fallen strung
  specter), Kết thúc lượt. Greyed rows say why (the existing `why` words). Placed beside the unit on the
  side with room. Focus opens on Tấn công.
- **Target preview** on the target: its HP bar shaded by the coming damage, a bubble with the range and
  the counter-blow ("5–7", "trả 3–4") and one warning word; the numbers equal the rules' `Preview`.
- Fixed HUD at most 4 elements under 15% of 1280×720 (goals, strip, toasts, hint). Toasts ≤ 3, 3 s,
  top right. Name tags only on the acting, hovered or targeted unit (≤ 3).
- No blood, gore, skulls or bodies; a fallen unit kneels and dims (alpha 0.5). Strings red from the head
  to the stage's anchor; a freed unit's string snaps, its knot turns gold, the gold light lifts it away.
- `language = vi` shows no English; `ui::MissingGlyphs` finds nothing in the campaign's screens.
- Large text: body 16 → 20 px, labels 12 → 15, menus 15 → 19; panels grow, the board does not.
- Plain colours (the mockups' values in `skin.json`) until the art agent's set is approved.
- Commits from **#363**.

---

### Task 1: Words out of the screens (#363)

Every label typed into today's match and fight screens moves to `match-words.json` / `campaign-words.json`
(vi default, en for development); a test walks the campaign screens' text bindings and finds no literal
English. Commit.

### Task 2: The large-text skin (#364)

`skin-large.json` (or a `scale` block in `skin.json`), swapped at once by `UiSystem` (engine: re-measure
open screens on a skin change); a settings value in prefs. Tests: the swap re-lays an open screen; ui-preview
of the campaign HUD at both sizes without overlap at 1280×720. Commit.

### Task 3: Keys in a key file (#365)

`keys-keyboard.json` (meaning → key), read by `MatchPlay`, the dialog and the café; prompts show the key's
name. Tests: a remapped key works; an unknown meaning is refused with its row. Commit.

### Task 4: Goals card, order strip, hint and log line (#366)

`campaign-hud.ui.json`; goals from the stage's aims with ☐/☑ and the trapped ✗; the strip with faces this
round, side rings (round ally, square enemy), the acting gold ring; hint line with keys; the last log
line and L for the log. Tests on `HudData`. Capture. Commit.

### Task 5: The command menu and back stack (#367)

`MenuFor(state, unit)` (rows, enabled, why, number keys), `command.ui.json` with the Kỹ năng and Vật phẩm
sub-menus (name, HP cost, a one-line note), placed beside the unit; P2 back stack (target → sub-menu →
menu → board; a move undone with Back while nothing else was done). Tests: rows per situation (Cắt dây
only beside a fallen strung specter); back stack order; keyboard-only and mouse-only scripted turns. Commit.

### Task 6: Target preview and unit cards (#368)

`PreviewFor(state, action)` from the rules' `Preview`; the shaded bar and bubble; the hover card, pinned
with I. Tests: preview equals the rules; card fields. Capture. Commit.

### Task 7: Units on the board (#369)

`Strings`, `Plates` (HP bar in the side's colour, tag rule), fallen pose and dim, the snap-and-lift of a
freed unit (0.8 s, from the `Freed` log entry), telegraph `Marks` (breath lane, opening windows, sandbags,
reinforcements, a round ahead), forms and tints from `cast.csv`. Tests on the draw lists. Captures with the
analog screen on. Commit.

### Task 8: Toasts and the enemy turn (#370)

A toast queue in game code (found items, ingredients, keepsakes, freed, level), repeated later on the stage
result; the enemy turn's hint ("Lượt của {name} · Giữ Space nhanh hơn · F bỏ qua"); P10: the notebook key
pauses beats (the notebook itself is plan 6; here a stub overlay). Tests. Commit.

### Task 9: Records (#371)

`docs/testing/s49-battle-hud.md` (§4 on Windows: play Stage 1 with the mouse only, then the keyboard only;
large text; analog screen on and off), ROADMAP, status. Ship.
