# The Last Curtain: UI art requests for the campaign (2026-11-23)

**For:** the art agent working on the user's PC (ComfyUI, SDXL base 1.0; birefnet, inpainting and
ExpressionEditor installed; no LoRA, ControlNet, IP-Adapter or upscaler; about 85 s a picture), with
Python available for compositing.
**Source:** the UI design in `docs/design/2026-11-23-campaign-ui-handoff.md` and `docs/design/ux/*.md`;
the mockups (canvas "The Last Curtain — campaign UI mockups",
https://claude.ai/artifact/JFnQALDHDUJQVnVjmzeukz, private to the user: ask them to share it).
**Existing art and its recipe:** `plugins/last-curtain/apps/LastCurtain/assets/art/README.md` (the
figures, icons and boon icons in `art/ui/boons/`). Follow its workflow unless this list says otherwise.
**Order:** section 0 (style board) first, for the user's approval; then section 1 (slice 1); the rest
later.

---

## Style for the UI

- **The game's world stays as it is:** Vietnamese oil pastel on toned paper (figures, backdrops).
- **The UI's frames and surfaces:** Vietnamese **lacquerware (sơn mài)**: deep red-black lacquer
  grounds with a soft depth, thin **gold-leaf** inlay lines, a faint crackle in the lacquer, an
  occasional eggshell (vỏ trứng) fleck. Restrained and dignified, not ornate.
- **Icons:** the existing icon style (oil pastel, 128 × 128, cut out), on no background, so they sit on
  the lacquer panels. Simple silhouettes that read at 32 px.
- **Mockup colours to match** (the game's `skin.json` will take its final values from the approved
  art): lacquer #170c0a, panel #24120f, raised #2f1815, gold leaf #d4a94a, eggshell text #f0e5cc,
  ally pearl blue #8fb7cf, enemy cinnabar #d65f47.
- **Never:** readable text or letters in any picture (labels are drawn by the engine), logos, brand
  marks, real posters, blood, skulls, bodies.

**Why some items are made in code, not SDXL:** SDXL can't draw crisp, exact frame edges, straight gold
lines or legible glyphs. Frames are **composited in Python** from SDXL *textures* (lacquer, gold leaf,
paper); button glyphs and key caps are **drawn as vector shapes**. SDXL makes surfaces, icons and
paintings.

---

## 0. Style board (do first; about 30 minutes of GPU)

One composite image at 1280 × 720 for the user to approve: a panel, a button (normal, focused,
disabled), the torn-ticket room tile (normal, next stage, unknown), 4 icons, one ingredient, and the
command menu, placed over the existing `backdrops/box_office` and over the café wall. Two variants:
**A** lacquer as described; **B** the same with warmer, browner lacquer. The user picks before section 1.

---

## 1. Slice 1 set

### 1.1 Textures (SDXL, opaque, then tiled or cropped in Python)

| File (under `art/ui/campaign/`) | Make at | Ships as | Prompt idea |
|---|---|---|---|
| `tex/lacquer_dark.png` | 1024 × 1024 | 512 × 512 tileable | "close-up surface of Vietnamese black lacquer (sơn mài), deep red-black, soft depth, faint crackle, no pattern, even light" |
| `tex/gold_leaf.png` | 1024 × 1024 | 512 × 512 tileable | "gold leaf sheet texture, slightly worn, uneven, warm, flat light" |
| `tex/eggshell.png` | 1024 × 1024 | 512 × 512 tileable | "crushed eggshell inlay in black lacquer, scattered fine pieces, sparse" |
| `tex/paper_plan.png` | 1344 × 768 | as is | "aged off-white architectural drawing paper, faint stains, no lines, no text" |

Tileable: offset by half and inpaint the seams.

### 1.2 Frames (composited in Python from 1.1, 9-slice)

Each a PNG with alpha and a `slices.json` entry (left, top, right, bottom in pixels) for `skin.json`'s
`image` + `slice`.

| File | Size | Look |
|---|---|---|
| `frame/panel.png` | 128 × 128, slice 24 | lacquer fill, 1 px gold-leaf line inset 6 px, 6 px radius |
| `frame/panel_gold.png` | 128 × 128, slice 24 | as panel, a 2 px gold line (selected / important) |
| `frame/button.png`, `button_focus.png`, `button_disabled.png` | 96 × 48, slice 16 | raised lacquer; focus: gold fill with dark text area; disabled: flat, dashed dull line |
| `frame/menu.png` | 128 × 128, slice 20 | the command menu box: panel with a soft drop shadow baked in |
| `frame/bubble.png` | 96 × 64, slice 16 | small panel with a cinnabar 1 px line (target preview) |
| `frame/toast.png` | 96 × 48, slice 16 | small panel with a pale green-gold line |
| `frame/nameplate.png` | 96 × 32, slice 12 | slim dark plate for names over units and guests |
| `frame/tab.png`, `tab_active.png` | 96 × 40, slice 14 | notebook tabs |
| `frame/ticket.png`, `ticket_next.png`, `ticket_unknown.png` | 512 × 192, slice 48 | a **torn theatre ticket**: notched half-circles in both short sides, a perforated dashed line 70 px from the right end, lacquer face; next: gold edge and a faint gold glow; unknown: darker, dull edge |

### 1.3 Icons (SDXL, cut out, 128 × 128; the existing icon recipe)

| File | Subject |
|---|---|
| `icon/mark_cleared.png` | a small gold tick brushed like a lacquer stroke |
| `icon/mark_next.png` | a gold lozenge (diamond) |
| `icon/mark_skirmish.png` | two crossed paper fans (not weapons) |
| `icon/mark_unknown.png` | a dim question-mark-shaped wisp of smoke (no letter: a curl with a dot) |
| `icon/upgrade_ready.png`, `upgrade_locked.png` | a lit gold bead; an unlit bead |
| `icon/ap_full.png`, `ap_empty.png` | a gold bead; an empty gold ring (also used at 9 px) |
| `icon/cmd_move.png`, `cmd_attack.png`, `cmd_skills.png`, `cmd_items.png`, `cmd_end.png`, `cmd_cut.png` | footprints; a raised fist with a fan; an open fan; a cloth bundle; an hourglass; scissors cutting a red thread |
| `icon/rite.png` | an incense stick with rising smoke (Lễ cầu hồn) |

### 1.4 Ingredients, drinks and the keepsake (SDXL, cut out, icon 128 × 128 and a 512 × 512 picture for the notebook)

| File | Subject |
|---|---|
| `item/gung` | a knob of old ginger root |
| `item/duong` | a small paper twist of cane sugar, the paper yellowed |
| `item/mut_gung` | candied ginger slices in a small glazed bowl |
| `item/nuoc_gung` | a small cup of ginger water, steam |
| `item/giay_gap_tu` | **the keepsake:** a small yellowed note folded in four, worn edges, no readable writing |

Picture prompts: "still life of <subject>, Vietnamese oil pastel on toned paper, soft lamplight,
plain background" (warm mood words, as for the café).

### 1.5 Controller and keyboard glyphs (vector, drawn by script, not SDXL)

Xbox **layout and colours** only, **no Xbox or Microsoft logo or wordmark**: A (green), B (red),
X (blue), Y (yellow) round buttons with the letter; LB, RB, LT, RT; View and Menu (generic two-squares
and three-lines symbols); d-pad (each direction); left and right stick, and stick press (L3/R3).
Keyboard: a key-cap shape with the key's label drawn in Be Vietnam Pro (Esc, Tab, Enter, Space, F, I,
L, Q, E, W, A, S, D, C, 1–6, arrows) and mouse left, right, middle, wheel.
Sizes: 64 × 64 source, shipped at 32 and 48 px; `art/ui/glyphs/<device>/<name>.png`, plus
`glyphs.json` mapping input names to files.

### 1.6 The world map

| File | Make at | Ships as | Notes |
|---|---|---|---|
| `map/floor_plan.png` | 1344 × 768 | as is | the theatre's ground floor as an **aged architectural plan** in faint gold and sepia lines on `paper_plan`: auditorium, foyer, corridor, box office, prop store, stage, wings. **No text.** SDXL for the paper and ink feel, then the room outlines traced in Python from the room rectangles in the stage data so the tickets sit on them |

---

## 2. Later (not slice 1)

- Ingredients and keepsakes for stages 2–12 (Dừa, Nước dừa and Tí's memory 4 first), as the content
  docs name them.
- Upper floor plans of the theatre as later stages open.
- Large-text versions are not needed: frames are 9-slice and glyphs vector.

## 3. Delivery

- Files under `plugins/last-curtain/apps/LastCurtain/assets/art/ui/campaign/` (and `art/ui/glyphs/`).
- A section "Campaign UI" in `art/README.md`: each file, its prompt, seed and picks, and its weak spots.
- A capture of the style board and of section 1 laid out, for the user.
- **The user approves the style board before section 1, and section 1 before anything is wired.**
- Commits as usual (`issue #UMBRA-<n>`), through the `Temp/lcship` route.
