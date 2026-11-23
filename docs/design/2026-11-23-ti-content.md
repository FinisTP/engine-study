# The Last Curtain: Bé Tí's content for slice 1 (draft, 2026-11-23)

**Status:** Claude's draft for **the user's review as a native speaker** (every Vietnamese line). Built
on `campaign-content-2026-11-22.md` (Tí: Speedster, Quảng Ngãi, "tui / răng / rứa", calls the player
`{anh/chị}`, curious and cheeky; never boasts about killing, never swears; topics: his mother's
cooking, Tết, the sea) and on the evening's design (memory drinks, keepsakes, upgrade versions).
Numbers are first values for the soak; names in brackets are proposals.

---

## 1. Who he is (unchanged)

A barefoot boy of 12 who ran messages between hamlets and counted patrols from the dunes. He promised
his mother he'd be home for Tết. He fell on the white sand at dawn with one message still behind his
ear. Freed in Stage 1 (Phòng vé); joins at level 3.

**Stats** (Speedster line, HP −2, Move +2): HP 16, Atk 4, Def 2, Spd 6, Move 12.

## 2. How his memories come back (the new loop)

| # | Memory | Brought back by | Where it comes from | Unlocks |
|---|---|---|---|---|
| 1 | **Tên mẹ** (his mother's name) | a talk with Lệ Thanh, no item | the café, right after Stage 1 (her new power) | **Tí joins** |
| 2 | **Lời hứa Tết** (the Tết promise) | **Mứt gừng** (ginger jam): Gừng ×1 + Đường ×1 | Gừng: random drop from rat specters (Phòng vé and its skirmishes); Đường: a spot in Hành lang chân dung (T3), once | upgrade of **Đưa tin** |
| 3 | **Lời nhắn** (the message) | the keepsake **Mẩu giấy gấp tư** (the folded note) | Phòng vé, inside the ticket window the Rat guards (take-once) | upgrade of **Ná cao su** |
| 4 | **Bãi cát trắng** (the white sand) | **Nước dừa** (coconut water): Dừa ×1 | Stage 2's area (Nhân's coconut village ties in), later | upgrade of **Chân sáo**; Tí complete |

- Memory 1 needs no item, so **the slice ends with Tí joining without a replay** (closes the slice
  handoff's open item).
- In the slice the player can reach memories 1–3; memory 4 waits for Stage 2.
- **Each upgrade has two versions, chosen inside the memory's scene** by how the player answers Tí
  (the last choice of the scene). The choice is final (confirm, P8), and it shapes how Tí remembers.
- **Gừng is contested on purpose:** the same ginger makes **Nước gừng** (ginger water, +2 max HP for
  anyone, cap 3 per stat per character, `options.csv`). Spend it on Tí's memory or on a boost.

## 3. Abilities and upgrades

| Ability | Base | Upgrade version A | Upgrade version B |
|---|---|---|---|
| **Ná cao su** (slingshot) | battle strike, 0 HP, power 3 | **[Báo động]**: the target loses 2 Speed until its next turn (acts later in the order) | **[Bắn trước]**: +3 power against a unit that hasn't acted this round |
| **Đưa tin** (carry the word) | board, 2 HP, range 6 (walk); gives an ally 1 of Tí's stored AP | **[Về kịp]**: the ally also gets +2 move this turn | **[Phần quà]**: the ally also heals 3 HP |
| **Chân sáo** (skipping feet) | passive: +3 move on a turn after he bides | **[Gió biển]**: +4 move instead, and hazard squares cost no extra | **[Dấu chân]**: allies who end their move next to him get +1 move next turn |

Which version a choice gives is in section 4 under each scene.

## 4. Lines (Vietnamese; for the user's review)

### 4.1 Stage 1, Phòng vé (Tí strung, khăn tang over his eyes)

| When | Who | Line |
|---|---|---|
| Stage start hint | Lệ Thanh | "Đứa nhỏ kia bị Tý trói rồi. Cắt dây cho em trước khi con Chuột ngã nghe {anh/chị}." |
| Tí first seen | Tí | "Đừng lại gần… tui phải chạy. Có người đang chờ tin." |
| Tí hit (once) | Tí | "Ui da… tui có làm chi mô…" |
| Tí falls (strung) | Tí | "Mạ ơi… tối quá…" |
| Cut free | Tí | "Dây… đứt rồi hả? Tui… tui không nhớ tui là ai nữa." |
| Recap if left strung | Lệ Thanh | "Đứa nhỏ vẫn còn bị trói. Mình phải quay lại phòng vé." |

### 4.2 Café, memory 1: Tên mẹ (Tí joins)

1. *(narration)* Lệ Thanh dắt một đứa nhỏ chân đất vào quán, đặt nó ngồi xuống bên bàn gần cửa sổ.
2. **Tí:** "Chị ơi, tui đói. Mà tui không nhớ nhà tui ở mô."
3. **Lệ Thanh:** "Từ từ thôi em. Nhớ từ điều gần nhất."
4. *(narration)* Lệ Thanh đặt tay lên tấm khăn tang trên mắt Tí. Nút khăn lỏng ra.
5. **Tí:** "Mạ… mạ tui tên Lụa. Chiều mô mạ cũng đứng đầu ngõ kêu tui về ăn cơm."
6. **Choice** (sets a fact for later lines, no upgrade):
   - "Mạ Lụa chắc đang chờ em đó." → fact `ti_hope`
   - "Đói thì ăn cái đã, Tí." → fact `ti_fed`
7. **Tí:** "{anh/chị} cho tui đi theo với nghe. Tui chạy lẹ lắm, không ai đuổi kịp mô!"
8. *(narration)* Tấm khăn rơi xuống. Quanh Tí ánh lên một màu xanh nhạt.

### 4.3 Café, memory 2: Lời hứa Tết (brewing Mứt gừng)

1. *(narration)* Mùi gừng sên đường bốc lên. Tí ngồi im, hai tay ôm chén.
2. **Tí:** "Mạ hứa… Tết ni tui về, mạ sên mứt gừng cho tui. Cay mà ngọt."
3. **Tí:** "Tui cũng hứa với mạ: chạy xong chuyến ni là tui về liền."
4. **Choice** (the upgrade of Đưa tin):
   - "Em về kịp mà, Tí." → **Về kịp**
   - "Mạ để phần em một hũ đó." → **Phần quà**
5. **Tí** (Về kịp): "Ừ hé. Tết ni tui không trễ nữa mô."
   **Tí** (Phần quà): "Một hũ thôi hả? Tui chia cho {anh/chị} nửa hũ."

### 4.4 Café, memory 3: Lời nhắn (the keepsake Mẩu giấy gấp tư)

1. *(narration)* Mẩu giấy ố vàng, gấp tư, mép đã sờn.
2. **Tí:** "Cái ni… của tui! Tui giắt sau tai mà."
3. **Tí:** "Chú dặn đưa tận tay, không được đọc. Mà tui biết trong đó ghi: 'Sáng mai chúng tới.'"
4. **Tí:** "Tui chạy suốt đêm. Tui có kịp không, {anh/chị}?"
5. **Choice** (the upgrade of Ná cao su):
   - "Em đưa tới nơi rồi, Tí." → **Báo động**
   - "Em đã chạy hết sức rồi." → **Bắn trước**
6. **Tí:** "Rứa thì được rồi." *(narration)* Tí gấp mẩu giấy lại, cất vào túi áo.

### 4.5 Café, memory 4: Bãi cát trắng (Nước dừa; after the slice)

1. *(narration)* Tí uống một hơi hết trái dừa, rồi ngồi nhìn ra cửa rất lâu.
2. **Tí:** "Cát ngoài bãi trắng lắm, trưa nóng rát chân. Tui hay chạy trên đó cho lẹ."
3. **Tí:** "Sáng hôm đó tui nằm xuống trên cát, nghe sóng. Tui không sợ mô."
4. **Tí:** "Tui chỉ tiếc chưa được ăn mứt mạ sên."
5. **Choice** (the upgrade of Chân sáo):
   - "Giờ em chạy đâu cũng được rồi." → **Gió biển**
   - "Em chạy trước, tụi {anh/chị} theo sau." → **Dấu chân**
6. *(narration)* Lệ Thanh đứng sau quầy, không nói gì. Lần đầu tiên, chị quay mặt đi.
   *(a seed for her betrayal)*

### 4.6 Café idle lines (as a regular, one per visit, chosen in order)

- "Quán ni có bánh tét không {anh/chị}? Tui thèm quá."
- "Tui đếm rồi: từ cửa tới quầy mười hai bước. Lần sau tui chạy chín bước thôi."
- "Cơm bà Mai nấu giống cơm mạ tui… mà mạ tui nêm mặn hơn chút."
- (if `ti_hope`) "Hồi nãy tui mơ thấy mạ đứng đầu ngõ. Mạ cười."
- (if `ti_fed`) "{anh/chị} còn cơm không? Tui ăn được thêm chén nữa."

### 4.7 Battle bubbles (one line per moment, short)

| Moment | Line |
|---|---|
| Đưa tin | "Tin tới rồi nè!" |
| Bide (Kết thúc lượt +1 AP) | "Để tui thở cái đã." |
| Ná cao su | "Trúng chưa? Trúng rồi!" |
| Falls | "Mạ ơi…" |
| Revived | "Tui còn chạy được!" |

## 5. Notebook entries (memories, at most 140 letters each)

1. **Tên mẹ:** "Mạ tên Lụa. Chiều nào mạ cũng đứng đầu ngõ kêu Tí về ăn cơm."
2. **Lời hứa Tết:** "Mạ hứa Tết về sẽ sên mứt gừng. Tí hứa chạy xong chuyến cuối là về."
3. **Lời nhắn:** "Mẩu giấy gấp tư giắt sau tai: 'Sáng mai chúng tới.' Tí chạy suốt đêm để đưa tới nơi."
4. **Bãi cát trắng:** "Tí nằm xuống trên bãi cát trắng khi trời vừa sáng, nghe tiếng sóng."

## 6. Items for the slice

| Item | Kind | Where | Uses | Used in |
|---|---|---|---|---|
| **Gừng** (ginger) | ingredient | random drop from rat specters (Phòng vé, its skirmishes) [call: 30%] | Mứt gừng; Nước gừng | café |
| **Đường** (sugar) | ingredient | Hành lang chân dung, one spot, once | Mứt gừng | café |
| **Mẩu giấy gấp tư** (the folded note) | keepsake | Phòng vé, in the ticket window the Rat guards, once | Tí's memory 3 | café |
| **Mứt gừng** (ginger jam) | memory drink (Tí) | recipe taught by Lệ Thanh after Stage 1 | memory 2, Đưa tin upgrade | café |
| **Nước gừng** (ginger water) | general drink | recipe taught with Mứt gừng | +2 max HP, anyone, **cap 3 per stat per character** (`options.csv`) | café |
| **Dừa**, **Nước dừa** | ingredient, memory drink | Stage 2's area (later) | memory 4 | café |

Short descriptions for the notebook:
- Gừng: "Củ gừng già, cay nồng. Bóng chuột trong phòng vé hay tha theo."
- Đường: "Một gói đường cát nhỏ, giấy gói đã ngả vàng."
- Mẩu giấy gấp tư: "Gấp tư, mép sờn. Có người đã giữ nó rất kỹ."
- Mứt gừng: "Gừng sên đường, cay mà ngọt."
- Nước gừng: "Ấm bụng, ấm tay. Thêm 2 HP tối đa."

## 7. For the data files

- `options.csv`: `boostCapPerStat,3,general drinks per stat per character (the user)`.
- Cast sheet: Tí's memories with their source (talk / recipe / keepsake), the scene ids, and the
  version each choice gives.
- Scene files: `ti_memory_1` … `ti_memory_4`, `ti_idle`, the Stage 1 moment lines; `{anh/chị}` and
  `{player}` tokens as in the slice handoff.
- Placeholder numbers (drop rate, upgrade powers) go to the campaign soak.

## 8. Open for the user

- Every line above: dialect, register, and whether "tui" or "con/em" suits a 12-year-old talking to
  the player.
- His mother's name (**Lụa**, my proposal) and the message's words ("Sáng mai chúng tới").
- The upgrade versions' names and effects.
