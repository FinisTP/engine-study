# The Last Curtain: pre-stage, stage result and game over

- **Status:** In Design · **Author:** Claude, with the user · **Last Updated:** 2026-11-23
- **Platform Target:** Windows PC; mouse + keyboard and gamepad (Xbox glyphs)
- **Template:** UX Spec (three small screens) · **Mockups:** artboards *Pre-stage*, *Stage result*,
  *Game over* · **Patterns:** P1–P3, P5, P8, P9, P12

## A. Pre-stage (Chuẩn bị)

### Purpose
The player arrives wanting to **choose who fights and check they're ready**, without a long setup.

### Context, navigation, entry and exit
From the world map after choosing a stage, replay or skirmish. World map → pre-stage → battle board.
Back returns to the world map; **Bắt đầu** starts the stage (nothing is saved or spent by entering).

### Layout
```
┌ Phòng vé · Màn 1 · gợi ý cấp 3 ───────────────────────────────────────────┐
│ Mục tiêu: Cắt dây cho bé Tí · Đánh bại Con Chuột                          │
│ ┌ who fights (max N) ───────────────────────────────┐ ┌ selected ───────┐ │
│ │ [✓ {player} Lv3] [✓ Bảo Lv4] [✓ Bà Mai Lv3] [ Tí ] │ │ abilities       │ │
│ └────────────────────────────────────────────────────┘ │ 4 item slots    │ │
│                                                         │ [Sửa trong Sổ tay]│
│                      [Bắt đầu  Enter]   [Quay lại  Esc]                    │
└───────────────────────────────────────────────────────────────────────────┘
```
- Character cards: face, name, level, HP; a check when chosen. The player is always chosen (locked).
- Selected panel: abilities (names only) and the 4 item slots; **Sửa trong Sổ tay** opens the
  notebook's Characters tab on that character (A1).
- A warning line only when something is likely wrong: "Không ai hồi sinh được đồng đội" (no chosen
  character has a revive item or a revive ability).

### States
Default (last party, or everyone if it fits within the stage's limit); full (the cap reached: unchosen cards dim with "Đội
đã đủ"); a recommended level above the party's: the level shows red with "cao hơn đội".

### Interaction map
| Component | Mouse | Keyboard | Gamepad |
|---|---|---|---|
| Character card | click toggles | arrows + Enter | d-pad + A |
| Sửa trong Sổ tay | click | Tab | View |
| Bắt đầu | click | F or focus + Enter | X or focus + A |
| Quay lại | right click / button | Esc | B |

### Data
Stage file (name, goal, level, party cap); save (characters, levels, slots, last party).

### Acceptance criteria
1. From the world map, a stage can be started in two presses (Enter on the room, Enter on Bắt đầu).
2. The player can't be unchosen.
3. The party cap is never exceeded.
4. The revive warning shows exactly when no chosen character has a revive item or ability.

## B. Stage result (Kết quả)

### Purpose
The player arrives wanting to **see what the fight earned them** and get back to the map or café.

### Context
After a won stage (or skirmish), as the closing dialog scene ends. Relieved, curious about rewards.

### Layout
```
            PHÒNG VÉ · ĐÃ QUA
  ┌ Rewards ───────────────────┐  ┌ Freed ───────────────────────┐
  │ +120 XP  ·  Bảo lên Lv 4   │  │ [Tí's face] Bé Tí được giải  │
  │ Vật phẩm: Băng gạc         │  │ thoát. Chờ bạn ở quán.        │
  │ Nguyên liệu: Gừng ×2 · Gạo │  └──────────────────────────────┘
  │ Kỷ vật: Cuống vé rách      │
  └────────────────────────────┘
        [Về quán  Enter]     [Bản đồ  Esc]
```
- One page; a second page only if more than 8 rewards (Q/E).
- A recap line names anyone left strung ("Bé Tí vẫn bị trói: chơi lại để cứu").
- Two buttons: **Về quán** (focused when something new can be brewed or someone waits) and **Bản đồ**.

### States
Won stage; won skirmish (no Freed panel); level-ups listed in the rewards; nothing found ("Không tìm
thấy gì thêm").

### Acceptance criteria
1. Every toast shown in the fight appears again here.
2. The focused button is Về quán when a brew or a talk is waiting, else Bản đồ.
3. Opens within 0.5 s of the closing scene.

## C. Game over (Thất bại)

### Purpose
The player arrives wanting to **understand why they lost and decide what next**, fast.

### Context
The living player fell, or a stage condition failed (Stage 1: the Rat fell with Tí strung). Stung;
should be able to retry in one press.

### Layout
```
                       THẤT BẠI
          {player} đã ngã.   (or: Bé Tí vẫn bị trói khi Con Chuột ngã.)
   ┌ Lễ cầu hồn ─────────────────────────────────────┐   (only while unused)
   │ Hồi sinh cả đội, đầy máu. Chỉ một lần trong game.│
   │                [Làm lễ]                         │
   └─────────────────────────────────────────────────┘
          [Thử lại  Enter]   [Về quán]   [Thoát]
```
- The reason is one line in plain words, from the rules.
- **Lễ cầu hồn** box appears only when unused and unlocked (after T3), and only for a player fall
  (not for a failed condition). Choosing it asks to confirm (P8); then the fight resumes where it
  stopped.
- T1: only Thử lại (before the café is unlocked).

### States
Player fell with the rite available; player fell, rite used; condition failed; T1.

### Interaction map
Thử lại focused by default (Enter / A). Làm lễ: click, or focus + Enter / A, then confirm. Về quán
and Thoát by focus. Back does nothing here (a choice is required).

### Acceptance criteria
1. Thử lại is one press away from the moment the screen opens.
2. The reason line always names what happened.
3. The rite box never shows after it was used, before T3, or for a failed condition.
4. Choosing the rite always asks to confirm, with the focus on Không.

## Shared

- **Transitions:** fade 0.3 s in and out; the game over screen fades the board to grey first (0.5 s).
- **Large text:** body 16 → 20 px; cards wrap names to two lines.
- **Localization:** button labels at most 14 letters; reason lines at most 70.
- **Party size:** each stage sets its own limit in its stage file (the user, 2026-11-23); the
  pre-stage header shows it ("Ra trận: 3/3").
