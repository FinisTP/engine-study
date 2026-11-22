# Concept art: the style preview (2026-11-23)

The art list's section 00. One of each kind of subject, so every look in the game can be approved
before anything is made in bulk. `2026-11-23-style-board.jpg` puts them together, with a mock-up of
the engine forms (`tools/art/style_board.py`, config `board.json`).

**How:**
- ComfyUI on the user's PC, SDXL base 1.0, dpmpp_2m karras, 30 steps, cfg 7.
- Outputs are in ComfyUI's `output/lastcurtain/concept/`; the cut-out masks are in
  `concept_mask/`.

**Picks** (name, seed):

| Subject | Pick | Notes |
|---|---|---|
| Player | `player_male_12` | Front-facing; no fan visible |
| Le Thanh | `lethanh_b_21` | "(bright red silk ao dai:1.4)", with kimono, hanfu and sash in the negative |
| Bé Tí (freed) | `ti_12` | |
| Bé Tí (bound) | `ti_bound_2` | The band and eye hollows painted by `kt/` and masked; inpaint 0.55, seed 2 |
| The Rat | `rat_b_33` | The zodiac ghost words, weighted |
| The hero | `hero_datdo_12` | Mirrored |
| The ghost | `ghost_attacker_12` | Mirrored, then the cloth inpainted (`ghost_bound_2`) |
| The ember bride | `ember_b_41` | Mirrored |
| The box office | `box_office_12` | |
| The café wall | `cafe_wall_b_52` | Furniture weighted out in the negative |
| The rescue | `cut_rescue_12` | |
| The card | `card_dawn_12` | |
| The icon | Drawn in code | SDXL failed 4 times at a slingshot |

**What did not work:**
- **Three-quarter view turned right:** SDXL gives front views or a left three-quarter (fixed by
  mirroring). A pose reference (img2img from one good three-quarter figure) is the fix for the set.
- **Vietnamese clothes:** áo dài and áo bà ba drift to kimono and hanfu unless the shape is spelled
  out and the others are negated.
- **Smaller misses:**
  - Tí's hat stays on his head.
  - The Rat has no props.
  - The ember bride is an orange áo dài, not embers.
  - Expressions (ExpressionEditor) are not tried yet.
- **The backdrops and still** came out as sepia charcoal drawings, and the figures as oil paintings.
