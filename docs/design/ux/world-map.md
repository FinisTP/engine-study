# The Last Curtain: world map (Bản đồ nhà hát)

- **Status:** In Design · **Author:** Claude, with the user · **Last Updated:** 2026-11-23 (rev. 2)
- **Platform Target:** Windows PC; mouse + keyboard and gamepad (Xbox glyphs)
- **Template:** UX Spec · **Mockup:** artboard *World map* · **Patterns:** P1–P4, P12, P13

## 1. Purpose & Player Need

The player opens it wanting to **decide where to fight next**: the next stage, a skirmish, or a
replay for something they still need.

## 2. Player Context on Arrival

From the café's door, or after a stage result. Calm, between fights; often holding a goal from the
notebook ("I need coconut").

## 3. Navigation Position

**The café is the hub; the world map is opened from it** (the user, 2026-11-23). The café is not a
room on the map: Back (Esc / B / right-click) or the **Về quán** button returns to the café.

## 4. Entry & Exit Points

| Source | Trigger | Context carried |
|---|---|---|
| Café | the door (E / A) | the save |
| Stage result | Bản đồ | the room just played is focused |

| Destination | Trigger | Irreversible changes |
|---|---|---|
| Pre-stage | Vào màn / Giao tranh / Chơi lại on a room | none |
| Café | Back, or Về quán | none |
| Notebook | Tab / View | none |

Pause is reached from the café (Esc there with nothing open) or with the gamepad's Menu here.

## 5. Layout Specification

```
┌ [◂ Về quán  Esc] (Có thể pha cho bé Tí)          Nhà hát Thành phố          [Sổ tay Tab] ┐
│ ┌────────────── floor plan: rooms as torn tickets ──────────────┐ ┌ selected room ──────┐ │
│ │ [ ? ]                [  ?  sân khấu chính  ]                   │ │ Phòng vé             │ │
│ │ [Tiền sảnh ✓] [Hành lang ✓] [PHÒNG VÉ ◆] [ ? ]                 │ │ Màn 1, gợi ý cấp 3   │ │
│ │ [Kho đạo cụ ⚔2]                                                │ │ Mục tiêu …           │ │
│ └────────────────────────────────────────────────────────────────┘ │ Còn ở đây …          │ │
│                                                                     │ [Vào màn  Enter]     │ │
└─────────────────────────────────────────────────────────────────────────────────────────┘
```

- **Room tiles carry only a name and one mark:** ✓ cleared, ◆ next stage, ⚔ skirmish (with how many
  are left), ? unknown. Everything specific (items left, the keepsake, what drops, goals) is in the
  **selected room panel**.
- **Room tiles are drawn as torn theatre tickets** (notched sides, a perforated edge): the one bold
  visual element of the map. The next stage's ticket is gold-edged and slightly larger.
- **Về quán** (top left) carries the café's one line of news, if any ("Có thể pha cho bé Tí").

### Skirmishes (the user, 2026-11-23)

- **A limited number become available after each stage** (set in data per stage). The map signals
  them with the ⚔ mark and the count left on the room's ticket, and a toast when new ones appear
  ("Có 2 trận giao tranh mới").
- A skirmish **reuses a past room's map or uses a new small map**; a new map appears as a new ticket.
- **XP from skirmishes is capped at the next stage's level cap**; once the party is there, the panel
  says "Đã đạt cấp tối đa trước màn tới" and the skirmish gives only its drops.
- Drops: that map's ingredients (random). Losing a skirmish keeps nothing, like a stage, and doesn't
  use it up.

### Selected room panel

| Room kind | Panel |
|---|---|
| Next stage | name, stage number and recommended level, goals, what can be found there, **Vào màn** |
| Cleared stage | finds left ("Còn 1 vật phẩm", keepsake found or "?"), what drops there, **Chơi lại** |
| Skirmish | level range, XP left before the cap, what drops, how many left, **Giao tranh** |
| Known, locked | what opens it |
| Unknown | nothing (the tile only) |

**Hierarchy:** the focused ticket → the panel's button → the next stage's gold ticket → the rest.

## 6. States & Variants

| State | Trigger | What changes |
|---|---|---|
| Default | arrive | focus on the next stage, or the room just played |
| New skirmishes | a stage won | ⚔ marks appear with a short pulse (once), toast |
| New room opened | a stage cleared | the ticket fades in from "?" over 0.6 s, toast "Mở: …" |
| XP cap reached | party at the next stage's cap | skirmish panels say so; drops still listed |

## 7. Interaction Map

| Component | Mouse | Keyboard | Gamepad | Outcome |
|---|---|---|---|---|
| Ticket | hover focuses, click confirms | arrows/WASD, Enter | stick/d-pad, A | pre-stage |
| Về quán | click | Esc | B | café |
| Notebook | click | Tab | View | notebook |

The room cursor moves to the nearest ticket in the pressed direction.

## 8. Data Requirements

Stage files: rooms, kinds, recommended level, goals, items and keepsakes per room, drops, skirmish
count and map, the XP cap. Save: cleared, taken, skirmishes used, party levels. Café: whether a brew
or a talk waits (for the Về quán line).

## 9. Events Fired

enterRoom(id, kind) → pre-stage; back → café.

## 10. Transitions & Animations

Opens from the café's door as a paper unfolding (0.35 s). The chosen ticket tears slightly and the
screen fades to the pre-stage (0.3 s); this tear is the screen's one orchestrated motion.

## 11. Input Method Completeness

Mouse ✔, keyboard ✔, gamepad ✔.

## 12. Accessibility

Room kinds differ by mark (✓ ◆ ⚔ ?) and edge style, not colour alone. Large text: ticket names
15 → 18 px; the panel widens to 340 px.

## 13. Localization

Ticket names at most 18 letters (two lines allowed); panel goals at most 80.

## 14. Acceptance Criteria

1. On arrival the focus is on the next stage, or on the room just played.
2. Back always returns to the café in one press.
3. Every ticket is reachable from every other with the arrows alone.
4. A tester can tell from the map alone where a skirmish is available and how many are left.
5. Items left and drops appear only in the panel, never on the tiles.
6. Large text fits at 1280×720.

## 15. Open Questions

None.
