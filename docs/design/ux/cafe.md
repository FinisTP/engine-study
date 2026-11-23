# The Last Curtain: the café (walking, talking, brewing)

- **Status:** In Design · **Author:** Claude, with the user · **Last Updated:** 2026-11-23
- **Platform Target:** Windows PC; mouse + keyboard and gamepad (Xbox glyphs)
- **Template:** UX Spec · **Mockups:** artboards *Café*, *Brewing* · **Patterns:** P1–P3, P7, P8, P10,
  P11, P12, P15

## 1. Purpose & Player Need

The player arrives wanting **a calm break after a fight**: hear the guests, and turn what they found
into a recruit's memory and a stronger ability.

## 2. Player Context on Arrival

From the world map or the stage result's "Về quán". Relaxed or relieved; often with a specific brew
in mind. No timers, nothing can be failed.

## 3. Navigation Position

**The café is the hub** (rev. 2): Tiếp tục on the title lands here; the door opens the world map, and
Back on the world map returns here. Pause opens here with Esc when nothing else is open.

## 4. Entry & Exit Points

| Source | Trigger | Context carried |
|---|---|---|
| Title | Tiếp tục | the save |
| World map | Back / Về quán | the save; who waits |
| Stage result | Về quán | the stage just won (for fight-memory lines) |

| Destination | Trigger | Irreversible changes |
|---|---|---|
| World map | walk to the door, E | none (saved automatically) |
| Pause | Esc / Menu | none |
| Brewing (overlay) | E at a recruit's table | — |
| Dialog scene (overlay) | E at a guest | answers that count are saved |
| Notebook | Tab / View | — |

## 5. Layout Specification

### 5.1 Walking (the room)
```
┌ Quán cà phê ┐        (narration line, when Lệ Thanh moves)        [Sổ tay Tab]
│                                                                         │
│      [Bảo]            [Lệ Thanh]            [Bé Tí  · Ký ức 2/4]        │
│     (table)                                  (table)            [Cửa]   │
│                         (you)                                           │
│                    [E] Pha chế cho bé Tí                                │
│           WASD đi · E nói chuyện / pha chế · Tab Sổ tay · Esc          │
└─────────────────────────────────────────────────────────────────────────┘
```
- **Removed from today's café HUD:** money, day, closing clock, the cup, drink orders and patience
  bars over tables.
- Name plates over guests only within 3 m of the walker, or the nearest guest; a recruit who can be
  brewed for shows a small gold line under the name ("Có thể pha chế").
- The narration line (Lệ Thanh's dance, arrivals) at the top, 4 s, never modal.
- One prompt at the bottom for the nearest spot (P15).

### 5.2 Brewing (overlay, opened at a recruit's table)
```
┌ [face] Pha chế cho bé Tí · Ký ức 2/4                         Esc đóng ┐
│ ┌ recipes ─────────────┐ ┌ preview ──────────────────────────────────┐ │
│ │ Mứt gừng   ✓ đủ      │ │ (one authored line from the memory)       │ │
│ │ Nước dừa   thiếu dừa │ │ [Che chở: + khiên 3]  [Đường xa: cách 3 ô] │ │
│ │ ── cho bất kỳ ai ──  │ │ Chọn một phiên bản cho Đưa tin.            │ │
│ │ Nước gừng  +2 HP     │ │ [Pha mứt gừng Enter]                       │ │
│ └──────────────────────┘ └───────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────────────┘
```
- **Recipes column:** this recruit's memory drinks first, then the general drinks. Each shows its
  name and one status: ✓ đủ / thiếu {ingredient} / đã pha. Ingredients needed are in the preview.
- **Preview:** for a memory drink, one authored line hinting the memory, the two upgrade versions
  side by side (the one this drink opens is gold, the other dim with what opens it), and the
  ingredients it uses. For a general drink: the boost, whom it can go to, and the cap used
  ("Bé Tí: 1/3").
- **Brewing a memory drink** plays the memory as a short dialog scene, then the upgrade is chosen
  (confirm, P8, because the version is final); a toast "Ký ức trở về: …".
- A general drink asks **for whom** (a list of characters with their cap), then applies.

### 5.3 Talking
E at a guest opens a dialog scene (slice design 7.11). Visitors' talks may fill a keepsake's or an
ingredient's place in the notebook (toast "Sổ tay: thêm gợi ý"). Talks can't be failed and have no
wrong answers.

**Hierarchy (walking):** the prompt → guests who have something new → the narration line → the rest.

## 6. States & Variants

| State | Trigger | What changes |
|---|---|---|
| Arrival | enter | camera on the walker; Lệ Thanh's arrival narration |
| Something new | a recruit can be brewed for / a guest has a new talk | gold line under that name; the world map's Về quán button carries the same line |
| Nothing new | everything done | no gold lines; Lệ Thanh's idle narration only |
| Brewing: missing ingredient | not enough | the brew button is disabled with "Thiếu: dừa (tìm ở …)" |
| Brewing: cap reached | general drink at 3/3 for everyone | the drink shows "đã đủ" |
| Recruit restored | all memories back | the recruit becomes a regular (sits, talks, no brewing) |

## 7. Interaction Map

| Component | Mouse | Keyboard | Gamepad | Outcome |
|---|---|---|---|---|
| Walk | click a spot | WASD/arrows | left stick | walker moves |
| Interact | click the guest/table | E / Space | A | talk or brewing |
| Recipe | click | ↑↓ + Enter | d-pad + A | preview |
| Upgrade version (after the memory) | click | ←→ + Enter | d-pad + A | confirm, then applied |
| Close overlay | right click | Esc | B | back to walking |
| Notebook | button | Tab | View | notebook |

## 8. Data Requirements

Save: shelf (ingredients, keepsakes), recipes known (Lệ Thanh teaches them), each recruit's
fragments and upgrade choices, general boosts used per character. Cast sheet: memory drinks per
recruit, upgrade versions. Scene files: memory scenes, talks.

## 9. Events Fired

talk(guest), brew(recipe, recruit), chooseVersion(ability, version), boost(drink, character).

## 10. Transitions & Animations

Brewing overlay: the room dims and blurs 0.25 s; the panel rises 20 px. A brew: a 1 s steam/pour
loop on the counter, then the memory scene.

## 11. Input Method Completeness

Mouse ✔, keyboard ✔, gamepad ✔ (every row above).

## 12. Accessibility

Statuses are words, not colours. Large text: body 16 → 20 px; the brewing panel widens to the
screen's width minus 80 px. Name plates have a dark backing for contrast on the warm room.

## 13. Localization

Recipe names at most 16 letters; version names at most 14; a version's effect at most 40; the
narration line at most 90 (wraps to two).

## 14. Acceptance Criteria

1. Nothing in the café HUD counts time or money.
2. From entering the café, a ready brew can be finished in under 30 s and four presses.
3. A missing ingredient always says where it can be found (or "?").
4. Choosing an upgrade version always asks to confirm; nothing else in the café does.
5. Walking stops while any overlay is open.
6. Large text fits at 1280×720.

## 15. Open Questions

None for the user. My calls: name plates only near the walker; a general drink asks for whom after
it is chosen.
