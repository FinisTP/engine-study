# The Last Curtain: battle board HUD

- **Status:** In Design · **Author:** Claude, with the user · **Last Updated:** 2026-11-23 (rev. 2)
- **Platform Target:** Windows PC, 1280×720 and up; mouse + keyboard and gamepad (Xbox glyphs)
- **Template:** HUD Design · **Mockup:** artboard *Battle board*
- **Patterns used:** `interaction-patterns.md` P1–P8, P10, P12, P14, P16

## 1. HUD Philosophy

The board is the game; the HUD frames it and gets out of the way. What a decision needs appears
**where the player is already looking** (beside the acting unit, on the target), not in a far
corner. The middle of the board is never covered by fixed HUD.

Replaces today's match HUD (`match-hud.ui.json`): a tag over every unit, an always-open forecast and
log, a faction score, repeated names in the order strip, mouse-only play.

Inspirations: Fire Emblem's command menu beside the unit, with Skills and Items as sub-menus; Into
the Breach and Triangle Strategy's damage shown on the target's own HP bar.

## 2. Information Architecture

| Information | Where | Tier |
|---|---|---|
| Stage goals, done or not | goals card, top left | must keep |
| Whose turn, who's next this round | order strip, top centre | must keep |
| Each unit's HP, side, form, AP | on the unit (bar, ring, strings, knot, pips) | must keep |
| The acting unit's commands | **command menu beside the unit** | your turn |
| An ability's cost and what it does | the sub-menu row and its one-line note | while choosing |
| The armed action's result | **on the target**: its HP bar shaded, a small bubble with the numbers | while targeting |
| Any unit's full numbers | a card beside the hovered unit (I / Y pins it) | on hover |
| Found items, ingredients, keepsakes | toasts, top right | 3 s |
| What to do now | hint line, bottom left | must keep |
| The log | one line under the hint; L opens all | can hide |
| Faction score, essence | — | removed in the campaign |

## 3. Layout Zones

```
┌──────────────┬─────────────────────────────────────┬──────────────┐
│ GOALS        │ ORDER STRIP (this round, AP pips)   │ TOASTS (≤3)  │
├──────────────┴─────────────────────────────────────┴──────────────┤
│                                                                   │
│        [unit]┌ Di chuyển      1 ┐┌ Quạt gió   1 AP ┐               │
│              │ Tấn công       2 ││ Hồi sức    2 HP │      [target] │
│              │ Kỹ năng      ▸ 3 │└─────────────────┘   ┌ 5–7 ┐    │
│              │ Vật phẩm     ▸ 4 │                      │ trả 3–4│   │
│              │ Kết thúc lượt  F │ (+1 AP if nothing done)          │
│              └──────────────────┘                                │
├───────────────────────┬───────────────────────────────────────────┤
│ HINT + LOG LINE       │                                           │
└───────────────────────┴───────────────────────────────────────────┘
```

Fixed HUD: goals, strip, toasts, hint. Everything else follows the unit or the target, placed on the
side with room (it flips left/right and up/down near the board's edges).

## 4. HUD Element Specifications

**Goals card (top left).** The stage's name small, one line per goal with ☐ / ☑. Done: a gold
flash; failed (Stage 1: the Rat fell with Tí strung): red ✗ just before the game over.

**Order strip (top centre).** Faces for this round only, acting first (gold ring), then "lượt sau";
ring by side; short name; AP pips. Fallen units left out. Hover: the unit's card; click: camera.

**Command menu (beside the acting unit; your turn).** A short vertical list with number keys:
1. **Di chuyển** (dim after moving; Back undoes the move while nothing else was done)
2. **Tấn công**
3. **Kỹ năng ▸** opens a sub-menu of this unit's abilities: name, cost (HP, AP), and a one-line
   note for the focused one; greyed ones say why
4. **Vật phẩm ▸** opens the unit's 4 slots
5. **Cắt dây** appears only when the player stands next to a fallen strung specter
6. **Kết thúc lượt** (F / X). **Biding is folded into it** (the user, 2026-11-23): ending the turn
   **without having moved or acted** is a bide and gives 1 AP; the button then reads
   "Kết thúc lượt +1 AP". After any move or action it reads plain "Kết thúc lượt".
After an action bought with AP, the menu reopens with "+1 lượt" at its top.

**Target preview (while an action is armed).** The target's own HP bar shows the coming damage as a
lighter segment; a small bubble beside it gives "5–7" and "trả 3–4" (the counter-blow), and one
warning word if any ("nhanh hơn: ×2"). No separate forecast panel.

**Unit card (hover / focus on any unit).** Beside the unit: name, class, level, HP numbers, Attack,
Defence, Speed, AP, status. I / Y pins it; Back unpins.

**Toasts (top right).** P12.

**Hint line + log line (bottom left).** One sentence and the keys as glyphs; the last log line under
it (L opens the full log).

**Enemy turn.** The command menu is gone; the hint line reads "Lượt của {name} · Giữ Space nhanh hơn
· F bỏ qua" (P16).

**On-unit marks.** A 32 px HP bar in the side's colour, AP pips, fallen units at half opacity with
slack strings if strung, a gold knot over recruit specters. A name tag only on the acting, hovered
or targeted unit (at most 3 at once); the boss's tag is never needed while targeting because the
target is already framed.

## 5. HUD States by Gameplay Context

| Context | Shown | Hidden / changed |
|---|---|---|
| Opening scene | dialog scene over the board | whole HUD |
| Your turn | command menu beside the acting unit | — |
| Kỹ năng / Vật phẩm open | sub-menu beside the menu | — |
| Moving | reach squares lit, path | command menu (returns after the move) |
| Targeting | valid targets lit, others dim; target preview | command menu |
| Enemy turn | hint line shows the enemy turn | command menu |
| Fight screen | the existing fight screen | whole HUD |
| Notebook / Pause | overlay; beats paused | HUD dimmed |
| Game over | game over screen | whole HUD |

## 6. Information Hierarchy

1. The board (units, forms, targets). 2. The command menu or the target preview. 3. Goals and order.
4. Toasts and the hint/log.

## 7. Visual Budget

Fixed HUD at most 4 elements (goals, strip, toasts, hint) and under 15% of the screen at 1280×720;
the command menu and previews are temporary and sit beside their unit.

## 8. Feedback & Notification Systems

Toasts (P12); goal flash; the living player under 30% HP: their bar and strip face pulse red slowly,
no text; sounds as today.

## 9. Platform Adaptation

Mouse: click the unit or anywhere on its menu; right-click backs out. Keyboard: 1–6 and F; arrows in
menus; Q/E cycle targets. Gamepad: A on the acting unit opens the menu; d-pad; A; B back; LB/RB
targets; X end turn. Prompts follow the last device. Full screen: edge pans; window: no.

## 10. Accessibility

Sides differ by ring shape as well as colour; the target preview is numbers, not colour only.
Large text: menus 15 → 19 px; the menu widens; the strip shows faces only (names in the tip).

## 11. Tuning Knobs

Toast duration and count, low-HP threshold, tag rule, strip length, menu side preference.

## 12. Acceptance Criteria

1. With 12 units at 1280×720, no fixed HUD element overlaps another and at most 3 tags show.
2. A new player can say whose turn it is and the stage's goals within 5 s.
3. With only a gamepad, only the keyboard or only the mouse, a tester can move, use a skill from the
   Kỹ năng sub-menu on a chosen target, and end the turn.
4. Ending the turn without moving or acting gives exactly 1 AP (up to the cap of 2); after a move it
   gives none, and the button's label says which.
5. The target preview's range matches the rules' `Preview`.
6. Opening the notebook mid-beat pauses the beats and resumes on the same beat.
7. Large text fits; no English with `language = vi`.

## 13. Open Questions

None.
