# The Last Curtain: the notebook (Sổ tay)

- **Status:** In Design · **Author:** Claude, with the user · **Last Updated:** 2026-11-23
- **Platform Target:** Windows PC; mouse + keyboard and gamepad (Xbox glyphs)
- **Template:** UX Spec · **Mockups:** artboards *Notebook — Characters*, *Notebook — Ingredients*
- **Patterns:** P1–P3, P5, P7, P9, P10, P13

## 1. Purpose & Player Need

The player opens it wanting to **look something up or set up the party**: what a character can do
and become, what they remember, where an ingredient or keepsake is found, and what it makes.

## 2. Player Context on Arrival

Voluntary, from any place (Tab / View). Mid-battle: a quick check, the fight paused. Elsewhere:
planning, unhurried.

## 3. Navigation Position

Any place → notebook (overlay). Pre-stage's "Sửa trong Sổ tay" opens it on a character.

## 4. Entry & Exit Points

| Source | Trigger | Context carried |
|---|---|---|
| World map, café, board, fight | Tab / View / the button | the place (battle → read-only) |
| Pre-stage | Sửa trong Sổ tay | that character |

| Destination | Trigger | Irreversible changes |
|---|---|---|
| Back to the place | Tab / View / Esc / B | item slot changes are saved at once (outside battle) |

## 5. Layout Specification

```
┌ Sổ tay   [Nhân vật] [Nguyên liệu] [Kỷ vật]  Q/E          (Trong trận · chỉ xem)   Tab đóng ┐
│ ┌ list ───────┐ ┌ detail ────────────────────────────────────┐ ┌ side ───────────────┐  │
│ │ {player}    │ │ face, name, class, level, stats             │ │ the selected entry  │  │
│ │ Bảo         │ │ Characters: skills list, bag, memories line │ │ Others: Dùng để /   │  │
│ │ Bà Mai      │ │                                             │ │         Tìm ở đâu   │  │
│ │ Bé Tí ◆     │ │                                             │ │                     │  │
│ │ ?           │ │                                             │ │                     │  │
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

### Tab 1: Nhân vật (Characters), nested (the user, 2026-11-23)

```
┌ list ──────┐ ┌ character ────────────────────────┐ ┌ selected skill ─────────────────┐
│ {player}   │ │ face  Bé Tí  Hỗ trợ, cấp 3        │ │ Đưa tin                          │
│ Bảo        │ │ HP 16  Công 4  Thủ 3  Tốc 7       │ │ Trao 1 AP cho đồng đội kề bên.   │
│ Bà Mai     │ │ Kỹ năng                           │ │ Nâng cấp (chọn một):             │
│ Bé Tí ◆    │ │  ▸ Đưa tin        ● có thể nâng   │ │  [Che chở]  Cần: mứt gừng ✓      │
│ ?          │ │    Chạy lẹ        ○ cần kỷ vật ?  │ │  [Đường xa] Cần: nước dừa        │
│            │ │    (cấp 5: kỹ năng mới)           │ │                                  │
│            │ │ Túi  [Băng gạc][Cơm][+][+]        │ │                                  │
│            │ │ Ký ức 2/4  (one line; ▸ opens)    │ │                                  │
└────────────┘ └───────────────────────────────────┘ └──────────────────────────────────┘
```

- **List:** met characters, then "?" for those not yet met.
- **Character column:** face, name, class, level and stats on two lines; **Ra trận màn tới**
  (outside battle); **Kỹ năng** as a short list (name and one status mark: ● can be upgraded now,
  ✓ upgraded, ○ locked); then **Túi** (4 slots; editable outside battle); then **Ký ức** as one
  line ("Ký ức 2/4") that opens the memories in the right panel when chosen. Memories matter less
  while planning, so they sit last.
- **Right panel = the selected entry.** A skill: what it does, the level it came at, and its upgrade
  as two versions side by side (chosen in gold; the one that can be brewed now outlined; locked ones
  say what opens them). The memories line: the restored entries in full and "?" with the drink or
  keepsake that brings each back. A slot: the item and what it does.
- Focus moves list → character column → right panel with → (d-pad →), back with ← / B.

### Tab 2: Nguyên liệu (Ingredients)
- **List:** each ingredient with its icon, count on the shelf and where it's found (short); "?"
  rows for ingredients not yet seen.
- **Detail:** name, count; **Tìm ở đâu** (rooms and how: a monster drop, a spot in the room; "?" for a
  room not yet opened; "(màn này)" when it's the current stage); **Dùng để** (each recipe and what it
  does). Where it can be used is one line only if it isn't the café ("Dùng được trong trận").

### Tab 3: Kỷ vật (Keepsakes)
- Same shape as ingredients. A keepsake not found: its silhouette and "?"; once its room is known
  (a visitor's hint or the room opened): the room's name; once found: its name, picture, the memory
  it opens and for whom.

**Hierarchy:** the selected entry's detail → the list → the tabs.

## 6. States & Variants

| State | Trigger | What changes |
|---|---|---|
| Outside battle | opened from map, café, pre-stage | slots and Ra trận editable |
| In battle | opened from board or fight | a red "Trong trận · chỉ xem" tag; nothing editable; beats paused |
| Empty tab | no keepsakes yet | one line: "Chưa có kỷ vật. Tìm trong các phòng." |
| New entry | something found since last opened | a gold dot on the tab and the entry, cleared when seen |

## 7. Interaction Map

| Component | Mouse | Keyboard | Gamepad | Outcome |
|---|---|---|---|---|
| Tabs | click | Q / E | LB / RB | switch tab |
| List entry | click | ↑ / ↓ | d-pad ↑↓ | detail |
| Into the detail | click | → | d-pad → | focus moves to slots / versions |
| Item slot (outside battle) | click | Enter | A | shelf picker |
| Ra trận | click | Space on it | A | toggle |
| Close | right click / button | Tab or Esc | View or B | back |

## 8. Data Requirements

Save (characters, levels, stats, abilities, upgrade choices, slots, shelf, memories, items and
keepsakes found, hints learned); cast sheet (abilities by level, upgrade versions and what opens
them, memory drinks); stage files (what drops where, items and keepsakes per room).

## 9. Events Fired

swapItem(character, slot, item), setFights(character, bool); none for browsing.

## 10. Transitions & Animations

Opens in 0.2 s (fade + 12 px rise), closes in 0.15 s; tab change has no animation (instant).

## 11. Input Method Completeness

Mouse ✔, keyboard ✔, gamepad ✔.

## 12. Accessibility

Chosen / available / locked differ by border style and words, not colour alone. Large text: body
16 → 20 px; the side column moves under the detail when it no longer fits.

## 13. Localization

Ability names at most 16 letters; version effects at most 40; memory entries at most 140.

## 14. Acceptance Criteria

1. Opens in one press from every place, including during an enemy's beat, and reopens on the last
   tab and entry.
2. In battle nothing in it can be changed, and the fight resumes on the same beat when it closes.
3. For any locked upgrade, a tester can find what unlocks it and where that is found within two
   tab switches.
4. Undiscovered characters, keepsakes and rooms show only "?".
5. Large text fits at 1280×720.

## 15. Open Questions

None.
