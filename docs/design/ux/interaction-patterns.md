# The Last Curtain: interaction patterns and screen map

- **Status:** In Design
- **Author:** Claude, with the user as creative director
- **Last Updated:** 2026-11-23 (rev. 2)
- **Platform Target:** Windows PC; mouse + keyboard and gamepad, both complete (the user, 2026-11-23)
- **Template:** Interaction Pattern Library

## 1. Overview

The campaign's screens, how they connect, and the rules every screen follows. Each screen's own
spec (`docs/design/ux/<screen>.md`) uses these patterns instead of inventing its own.

### 1.1 Screen map

```
                         ┌──────────┐
                         │  Title   │  Continue · New game (name, gender) · Settings · Quit
                         └────┬─────┘
                              ▼
                        ┌──────────────┐
                        │  CAFÉ (hub)  │ walk, talk, brew
                        └──────┬───────┘
                   the door ▼  ▲ Back / Về quán
   ┌──────────── WORLD MAP (the theatre's floor plan) ────────────┐
   │ rooms: next stage · cleared (replay) · skirmish (⚔) · ?      │
   └───┬───────────────────────────────────────────────────────────┘
       │ pick a room
       ▼
  ┌──────────┐
  │ Pre-stage│
  │ (who     │
  │  fights) │
  └────┬─────┘
       ▼
  ┌──────────────┐  a battle starts  ┌──────────────┐
  │ BATTLE BOARD │ ────────────────► │ FIGHT SCREEN │ (3 rounds, over the board)
  │   + HUD      │ ◄──────────────── └──────────────┘
  └────┬─────────┘
       ▼ win / game over
  ┌──────────────┐
  │ Stage result │ XP · items · ingredients · keepsake · freed → café or world map
  └──────────────┘

  OVERLAYS, open on top of any screen above:
  Dialog scene · Notebook (Sổ tay) · Pause (Settings, Controls) · Game over
```

### 1.2 Layering rules

1. **Three layers:** a *place* (world map, café, board), its *HUD*, and at most **one overlay** on
   top. The only exception: a dialog scene may open over the fight screen.
2. **Esc / gamepad B always closes the top overlay.** With no overlay open, it opens Pause.
3. **The notebook opens from any place with one key, mid-battle included, on any turn.** It changes
   nothing: the fight's beats pause while it is open and resume when it closes. In battle it is
   read-only; outside battle you brew and assign from it.
4. **Saving is automatic** on the world map and in the café. In a stage, Pause offers **Give up**
   (back to the world map, the stage lost) instead of Save.

### 1.3 Decisions (the user, 2026-11-23)

- **The party is set up inside the notebook (A1).** Its Characters tab shows each character's
  abilities, upgrades, memories and 4 item slots, and outside battle you edit them there. The
  pre-stage screen only picks who fights and links to the notebook.
- **The world map is a painted 2D floor plan of the theatre (B1).** Rooms are clicked or chosen with
  the gamepad; locked rooms are silhouettes with "?", matching the notebook's unknown rooms.
- **The café is the hub (the user, rev. 2):** Continue lands in the café; its door opens the world
  map; Back on the world map returns to the café. The café is not a room on the map.

## 2. Pattern Catalog

| ID | Pattern | Where | In one line |
|---|---|---|---|
| P1 | Input map | everywhere | One table of what each key, mouse button and gamepad button does, the same in every place |
| P2 | Back stack | everywhere | Esc / B / right-click close the top layer: target → menu → overlay → Pause |
| P3 | Menu focus | all menus | The engine's focus: arrows / d-pad / stick move it, Enter / Space / A confirm, the mouse hovers |
| P4 | Board cursor | world map, battle board | A cursor on squares (or rooms) moved one at a time by keys and the gamepad, by hovering with the mouse |
| P5 | Info on hover/focus | everywhere | Info shows beside what it is about (a card by the unit, the right panel in lists); I / Y pins it |
| P6 | Command menu | battle board | A menu beside the acting unit, Kỹ năng and Vật phẩm nested; number keys; greyed commands say why |
| P7 | Preview before commit | battle board, notebook | Damage, healing, costs, or what a brew does, shown before you confirm |
| P8 | Confirm only when it can't be undone | anywhere | A yes/no only for Give up, the rite, spending a keepsake, choosing an upgrade |
| P9 | Tabs | notebook, stage result | Q / E (LB / RB) switch tabs; the last tab is remembered |
| P10 | Overlay pause | notebook, Pause, dialog | An overlay stops the fight's beats and the café's walking; nothing changes until it closes |
| P11 | Text advance | dialog scenes | Click / Space / A completes the line, then advances; hold to hurry; Bỏ qua stops at a choice; Nhật ký shows the log |
| P12 | Toasts | every place | Short notices ("Nhận: Gừng ×1") in one queue at the top right, never over the board's middle |
| P13 | Locked and unknown | world map, notebook | "?" for what isn't discovered, with a hint where; known-but-locked shows what unlocks it |
| P14 | Camera | board, café | Right stick / middle-drag pans, triggers / wheel zoom, C / R3 recentres on the acting unit |
| P15 | Walk and interact | café | WASD / stick walks; a prompt shows what E / Space / A does at the nearest spot |
| P16 | Hurry the enemy turn | battle board | Hold Space / A speeds the beats; Skip jumps to your next turn |
| P17 | Text entry | new game | The engine's field (Unikey Vietnamese); a gamepad gets an on-screen keyboard |

Decisions (the user, 2026-11-23): **right-click cancels (C1)**; inspecting is by hovering (P5).
**The notebook opens with Tab, View/Select on the gamepad (D1).**

## 3. Patterns

### P1. Input map

The same meaning in every place; a place may leave a row unused but never gives it another meaning.

| Meaning | Mouse + keyboard | Gamepad (Xbox names) |
|---|---|---|
| Move the cursor or focus | hover; arrows or WASD | left stick, d-pad |
| Confirm, choose, act | left click; Enter or Space | A |
| Back, cancel | right click; Esc | B |
| Pause (when nothing is open) | Esc | Menu |
| Notebook | Tab | View |
| Previous / next (tab, target, unit) | Q / E | LB / RB |
| Pin the info panel | I (the mouse shows it by hovering) | Y |
| Pass time: end turn on your turn, skip on theirs | F (button: Kết thúc lượt / Bỏ qua) | X |
| Hurry beats or text | hold Space | hold A |
| Camera pan / zoom / recentre | middle-drag or edge of screen / wheel / C | right stick / LT, RT / R3 |
| Action shortcuts (board) | 1–9 | (action menu with the d-pad) |
| Log | L | Pause → Nhật ký |

Rules:
- **Prompts show the device last used:** a key name after a keyboard press, a button glyph after a
  gamepad press, nothing extra for the mouse.
- Keys are data (a key file per device), so they can be remapped later without code.
- The mouse alone can play everything; keys and the gamepad can also play everything.

### P2. Back stack

Esc, B and right-click step back one level at a time, never two:
1. an armed action's targeting → the command menu (a sub-menu → its menu);
2. the command menu → the board (the unit stays selected; its move can still be undone with Back
   while nothing else was done);
3. any overlay (notebook, dialog log, info pinned) → closes;
4. nothing open → Pause (on the world map: back to the café; Pause is the gamepad's Menu there). In
   Pause, Back resumes.
A dialog scene ignores Back except to close its log; skipping is its own button (P11).

### P3. Menu focus

The engine's rules (`engine/ui/include/UI.md`, "Focus and input"): choosable widgets collected in
reading order, up/left and down/right move the focus (wrapping), the mouse focuses what it is over,
confirm sends the action. Every menu opens with the focus on its most likely choice (the next stage
on the world map, Tấn công in the command menu, Tiếp tục on the title).

### P4. Board cursor

- One cursor square on the battle board (one room on the world map), drawn as a gold frame.
- The mouse puts it under the pointer. Arrows / WASD / left stick / d-pad move it one square,
  repeating while held (0.25 s, then every 0.08 s); the camera follows when it nears an edge.
- On your turn the cursor starts on the acting unit. Confirm on it opens the command menu (P6).
- While an action is armed, the cursor jumps to the nearest valid target, and Q / E (LB / RB) cycle
  through valid targets only; invalid squares are dim and the cursor can't confirm on them.
- While moving, the path to the cursor's square is drawn (gold within reach, red beyond).

### P5. Info on hover/focus

- **Info appears beside what it is about** (rev. 2): on the board, a card beside the hovered unit
  (name, level, HP, Attack, Defence, Speed, AP, status) or square (its kind, an item); in lists, the
  right-hand panel shows the selected entry. Nothing shows for an empty floor square.
- While an action is armed, the result shows **on the target** (P7), not in a panel.
- I / Y pins it open as a larger card with everything (abilities, upgrades, memories, the item's
  uses); Back unpins.
- Tooltips (`tip`) still explain disabled buttons.

### P6. Command menu

- **A command menu beside the acting unit** (rev. 2): Di chuyển, Tấn công, **Kỹ năng ▸** (a sub-menu
  of abilities with HP and AP costs), **Vật phẩm ▸** (the 4 slots), Cắt dây (only next to a fallen
  strung specter), Kết thúc lượt. Number keys 1–6; F ends the turn.
- **Biding is folded into Kết thúc lượt:** ending the turn without having moved or acted gives 1 AP,
  and the button says so ("Kết thúc lượt +1 AP").
- A greyed action shows why on hover/focus (the existing `why` words).
- Gamepad: A on the acting unit opens the menu; the d-pad picks; A arms or opens a sub-menu; B
  steps back.
- After an action bought with AP, the menu reopens with "+1 lượt" at its top.

### P7. Preview before commit

Before any action that changes numbers, its result shows: on the board, the target's own HP bar
shades the coming damage and a small bubble beside it gives the range and the counter-blow; a heal's
amount the same way; an ability's costs (HP, AP) and effect; in the café, what a brew makes and
for whom (the upgrade's two versions side by side, or the boost and how many of its cap are used).

### P8. Confirm only when it can't be undone

A small yes/no (focus on **No**) only for: Give up (the stage is lost), Lễ cầu hồn (once a game),
spending a keepsake, choosing an upgrade's version.
Everything else acts at once; walking can be undone with Back until another action is taken.

### P9. Tabs

The notebook's tabs (Nhân vật, Nguyên liệu, Kỷ vật) and the stage result's pages. Q / E or LB / RB,
or click. The notebook reopens on the tab and character last shown.

### P10. Overlay pause

While an overlay is open: the fight's beats stop, bots don't act, the café's walker stands. In a
dialog scene inside a fight, the scene waits for the current action to finish before it opens.

### P11. Text advance

Dialog scenes (slice design 7.11): click / Space / A completes the line, then the next; hold Space
or A to hurry; **Bỏ qua** (button, or X) skips to the end but stops at a choice; **Nhật ký** (button,
or L / Y) shows the lines so far. Choices use menu focus (P3).

### P12. Toasts

At most 3 at a time, top right under the stage's aim, each 3 s, newest on top, queued beyond that:
items, ingredients and keepsakes found, a memory restored, a level reached. Never modal. Nothing a
toast says is lost: the stage result and the notebook repeat it.

### P13. Locked and unknown

- Undiscovered: a silhouette and "?" (a room on the world map, a keepsake, an ingredient's source).
- Discovered but locked: shown dim with what unlocks it ("Cần: mứt gừng", "Mở sau Phòng vé").
- Hints come from the game (a visitor's talk fills a keepsake's room, design note on visitors).

### P14. Camera

On the board and in the café: middle-drag pans; **the screen's edge pans only in full screen, never in a
window** (the user),
wheel or LT / RT zoom, C or R3 recentres on the acting unit (the café: on the walker). The camera
follows the told action (the existing focus) unless the player has moved it during this beat.

### P15. Walk and interact

Café: WASD / arrows / left stick walk; a click walks to the spot. The nearest spot within reach
shows a prompt at the bottom ("[E] Nói chuyện với Bà Mai", "[E] Pha chế"); E / Space / A does it.
Walking stops while an overlay is open.

### P16. Hurry the enemy turn

Hold Space / A: beats run at a quarter of their wait. X / F / the Bỏ qua button: everything is told at
once and the game runs to your next turn (the existing Skip). Dialog scenes set off by a moment still
play when reached (each once per attempt).

### P17. Text entry

The engine's `field` widget with the keyboard (Vietnamese through Unikey). With a gamepad, focusing
the field opens an **on-screen keyboard** with Vietnamese letters and tone marks (a gap, section 4).

## 4. Gaps & Patterns Needed

- **Board cursor for keys and gamepad (P4):** new; today the board is mouse-only.
- **Device-aware prompts (P1):** track the last device used; Xbox-style button glyph art.
- **Large text (Settings):** a second skin size the UI swaps at once (`skin.json` font sizes).
- **Key files per device (P1):** today keys are in code (`MatchApp.cpp`, `CafeApp.cpp`, `Main.cpp`).
- **On-screen keyboard with Vietnamese tone marks (P17):** new widget or screen.
- **Toast queue (P12):** a list widget fed by the game is enough; the queue lives in game code.
- **Screen words in words files:** labels typed into today's screens ("Your side", "Forecast",
  "Log") move to the words files so Vietnamese replaces them.
- **Pause without player save:** today's pause offers Save mid-match; the campaign's offers Give up.
- The engine already maps gamepad buttons and sticks (`engine/platform/include/Input.hpp`) and its
  menus already take focus input; P3 needs no new work.

## 5. Open Questions

Answered (the user, 2026-11-23):
- Edge-of-screen panning: **only in full screen**, never in a window.
- **A large-text option** (Cỡ chữ: Thường / Lớn) in Settings; every screen spec lists what it does in
  Lớn (body text 16 → 20 px, labels 12 → 15 px; panels grow, the board does not).
- **Gamepad glyphs: Xbox style only.**

Still open: none.
