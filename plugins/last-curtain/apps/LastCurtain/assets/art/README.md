# The Last Curtain's pictures

Made on the user's PC (2026-11-15) with ComfyUI and SDXL base 1.0 (`sd_xl_base_1.0.safetensors`),
28 steps, CFG 6.5, dpmpp_2m karras, 1024 px. Cut in with `tools/art/cut.py` (rooms, events,
portraits: 512 px JPEG; items: 256 px PNG with the cream background keyed out; cards: 256 px PNG, whole, as they stand in frames).
`blood_lut.png` and `drips.png` in `ui/` are drawn by `tools/art/make_mood_art.py`, not generated.

| Set | Count | How |
|---|---|---|
| `rooms/` | 62 | img2img at denoise 0.88 over a grainy sketch of the room's kind colour with four doorways (top-down) |
| `events/` | 33 | text to image |
| `portraits/` | 7 | text to image |
| `cards/` | 10 | text to image, whole |
| `items/` | 19 | text to image, keyed out |
| `ui/` | 3 + 2 | `panel`, `paper`, `title` text to image; `blood_lut`, `drips` drawn |
| `plaques/` | 62 | drawn by `tools/art/make_plaques.py` |
| `sky/` | 2 | text to image at 1536 x 640, seed 22 of 11/22/33 (2026-11-15) |

- Prompts and seeds: `tools/art/lastcurtain_prompts2.json` (the "dread" style the user asked for:
  subtly uncanny, 1960s horror-poster gouache, jade and blood red).
- Second tries, with why the first was rejected: `tools/art/lastcurtain_redos.json` (rooms drawn
  from the side or too bloody; events with garbled text; items that were not the object). Shoes
  and sword were tried a second time for blood drips and a landscape background (seeds 8211,
  8212, the items prompt with "still life of ...").
- The last try of each picture is used. The pictures carry no gore (the user): the blood of the
  night comes from the shaders after the Turn.
- The Saigon Opera House is named in the room prompts by its real name; nothing is drawn from any
  board game.

Sky prompts (SDXL base, dpmpp_2m karras, 30 steps, cfg 6.5, the dread style appended; negative: the
dread negative plus "buildings in foreground, people, ground, street, frame, border"):

- `before`: wide panoramic night sky over old Saigon at the end of dusk, last violet and burnt orange
  glow low on the horizon fading into deep indigo night above, first faint stars, thin drifting
  clouds, distant silhouettes of colonial rooftops and tamarind trees only at the very bottom edge,
  empty sky fills the image.
- `after`: wide panoramic midnight sky over old Saigon, a huge blood red full moon behind heavy
  churning storm clouds, clouds lit crimson from beneath, faint lightning in the distance,
  oppressive black and dark red sky, distant silhouettes of colonial rooftops only at the very
  bottom edge, empty sky fills the image.

Seeds 11 and 33 were rejected: 11 showed a flat field (before) and a pale moon (after); 33 a flat
green poster sky.

`items/corpse.png` (the battle royale's "singer's body" quest, 2026-11-16): text to image, seed 51 of
51/52/53 ("game item icon: a long rolled bundle of faded red velvet cloth tied with rope at both ends,
lying horizontally ... plain flat cream background", short dread style; negative adds gore, bodies
and scenery). Seeds 41 and 42 with the item prompt drew whole curtained rooms.

## Sprites (plan D, 2026-11-18)

`sprites/<guest>/` holds a guest's 2D sprite: `body_<face>.png` (full body, 768 px tall) and
`face_<face>.png` (head and shoulders, 512 px wide) for the faces calm, angry, neardead, happy,
scheming, afraid and talk, all with alpha. Huy is the template; Bao, Khang, Mai and Lan have their own since 2026-11-19 (below); the other guests use Huy's (tinted) until drawn.

The look (the user, 2026-11-18): **Vietnamese oil pastel**, silent, morbid and uncanny; every guest
**faces right** (the game mirrors for left) and wears a **white khan tang tied over the eyes**, with
dark hollow shadows soaked through where the eyes are; the rest of the face carries the feeling.
No hoods. This replaces the 1960s poster style for characters only.

Style words: "Vietnamese oil pastel drawing on toned paper, thick waxy smudged strokes, visible
paper grain, muted desaturated earth colours, raw umber, ash grey, faded olive, dull blood red
accents, dim light, silent, morbid, uncanny, eerie stillness, unsettling, melancholic".

How Huy was made (ComfyUI on the user's PC, SDXL base, dpmpp_2m karras):

1. Body: text to image, 832 x 1216, 30 steps, cfg 6.5, seed 9601 of 9501-9801: "full length
   standing figure of a single young Vietnamese man, feet and shoes visible, small in the frame with
   empty space around, he faces right in side profile, short black hair, bare head, olive green
   field jacket with the hood down hanging behind his neck, dark trousers, worn canvas sneakers,
   arms hanging at his sides, pale ashen skin, plain dim greyish paper background" + the style.
   Negative: "hood up, hooded head, close-up, portrait crop, cropped legs, cropped feet, photo, 3d
   render, anime, cartoon, bright saturated colours, text, watermark, frame, border, multiple
   figures, scenery, gore, blood, deformed". 9601 had no hood at all; it looked left, so it was mirrored.
2. Blindfold: SDXL will not draw one from the main prompt. Paint a rough off-white band and two
   dark blurred smudges over the eyes, then repaint only that area (SetLatentNoiseMask, denoise
   0.55, seed 2: "a white cotton mourning cloth, khan tang, tied tightly as a blindfold over his
   eyes ... two dark hollow sunken shadows seep through the white cloth where his eyes are"). The
   repaint washes the smudges out, so paint them again (dark, blurred, with grain) and blend with a
   second repaint of the band at denoise 0.32.
3. Portrait: crop 300 x 300 at (310, 105), scale to 1024, img2img denoise 0.25.
4. Faces: ExpressionEditor with the `blindfolded` values in `tools/art/lastcurtain_expressions.json`
   (no blink, pupil or wink; mouth, brows and head only). Scale the body 2x first. LivePortrait still
   finds the face behind the cloth.
5. Cut-out: birefnet on each changed picture (not the original, or a moved head leaves a fringe).
6. `python3 tools/art/pack_sprites.py <comfy output dir> huy <this folder>`.

A guest whose old portrait is only a head and shoulders can instead be extended downward (place
the cut-out on an 832 x 1216 canvas, mark the lower part transparent, VAEEncodeForInpaint, denoise 1):
that kept Le Thanh the same person in 2 of 3 tries.

### The faction match's cast: Bao, Khang, Mai, Lan (2026-11-19, overnight)

The user asked for the other starting characters drawn so none needs Huy's tinted sprite; Khang is
now a boy of 12 and Lan a woman of 26 (`docs/design/cast.md`). How they were made:

1. Pose: plain text to image gave front views. Instead img2img from Huy's calm body on a grey
   832 x 1216 page (denoise 0.72 / 0.8 / 0.86, seeds 11-13), the character in the prompt plus "in side
   profile facing right, seen from the side", negative "facing the viewer, front view". Picks: Bao
   0.8 (faced left: mirrored), Khang 0.8, Mai 0.72, Lan 0.72. Same scale and stance as Huy.
2. Blindfold: measure the eye on a 4x zoom (a coarse grid misled twice). Paint the band and smudge,
   mask the band + 10 px, SetLatentNoiseMask. Bao kept a wrapped cloth at 0.55 (seed 3); Khang kept
   it at 0.4 (seed 2); for Mai and Lan SDXL dropped it even at 0.4, so a shaded band (light middle,
   darker edges, fold strokes) was painted and only blended at 0.28 (seed 3). Then the eye smudge
   painted again and blended at 0.32 (Bao seed 6, Khang seed 5).
3. Portrait: crop (eye x - 170, eye y - 100) to (eye x + 90, eye y + 160), scale to 1024, img2img 0.25 (seed 21).
4. Faces: `blindfolded` values on the portrait and on the body scaled 2x; calm is the plain picture.
   Cut out with birefnet; packed with `pack_sprites.py`.

Known weak points: Khang and Lan are in strict profile, so LivePortrait moves their mouths only a
little; Mai's, Khang's and Lan's bands are flatter than Huy's and Bao's wrapped cloth.

### The specter: a giant spider (2026-11-18)

`sprites/specter/`: the user asked for an eldritch specter whose face is only a mask making eerie
expressions, then for something scarier, then for it to be like a giant spider. How it was made:

1. Body: text to image, 1216 x 832, cfg 7, seed 5201 of 5101-5401: "a single enormous eldritch spider
   seen from the side facing right, eight impossibly long thin jointed black legs arching high above
   its body, its head is a cracked white porcelain theatre mask ..., its swollen body covered in long
   dripping black hair like strands" + the style + "eldritch horror, overwhelming dread". SDXL drew
   spider eyes, never the mask, and a front view (fine for a boss that faces the audience).
2. Mask: a bone-white oval at (548-668, 272-412), two black eye holes and a red mouth painted on, then
   img2img at denoise 0.5 (seed 32) with "an enormous eldritch spider whose head is a cracked white
   porcelain theatre mask ..." and a negative of "spider eyes, eyeballs, skull, teeth, beak".
3. Legs: the legs ran off the bottom, so ImagePadForOutpaint added 256 px below (feathering 40) and
   VAEEncodeForInpaint, denoise 1, seed 41, painted the leg tips resting on a floor.
4. Faces: portrait (a 360 x 360 crop at (429, 167), scaled to 1024) with the `specter` values in
   `tools/art/lastcurtain_expressions.json`. Body (scaled 2x) with only the mouth and eye values (no
   head turns: on the full body they drag the body and paper along; the motions turn the sprite).
5. The mask is the paper's colour, so birefnet erases it in changed frames: every body frame takes the
   calm frame's outline plus a solid oval over the mask (1090-1346, 540-830 at 2x), and its colour from
   the changed frame only around the head.

`stage/stage.jpg`: the duel's backdrop, seed 7121, "an empty old opera house stage seen from the
stalls, heavy faded red velvet curtains ..., a single cold pool of light ..." + the style.

## UI art for the faction match (2026-11-20, `art/ui/match/`)

Made in the user's ComfyUI (SDXL base 1.0, dpmpp_2m karras, 30 steps, cfg 7) from prompts asking
for "game UI asset ... art deco ... tarnished brass ... straight front view, symmetrical, isolated on
plain black background", two seeds each (11, 23); the outputs are in ComfyUI's `output/lc_ui/`.
Kept: `backdrop.jpg` (the empty opera house, seed 23, the pick screen), `banner.png` (seed 23, the
turn banner, 9-sliced), `card.png` (seed 11, the battle's fighter cards, 9-sliced). Turned down:
the panels (too heavy for the HUD, the user wants it light), the bubbles (not clean), the first
banner (it drew letters), the first icons (each sat in a busy framed medallion). Icons are being
redone as plain white silhouettes on black, to cut out by brightness and tint in the skin.

Icons (`art/ui/match/icons/`): asked for "minimal flat game icon, solid pure white silhouette ... plain
pure black background" (seed 5); each cropped to one glyph (three came as 2x2 grids), inverted where
the model drew dark on light, cut out by brightness into a white glyph with alpha, 128x128. The skin
tints them (`iconGold`, `iconRed`, `iconPale`); the table stands the "?" and the chest in optional
rooms' corners.

### Frames, fight backdrops and wallpaper (2026-11-20, the user's notes)

Same ComfyUI and model. Frames: "thin rectangular decorative frame border ... art deco ... thin
tarnished antique brass lines ... isolated on plain pure black background, empty pure black
interior", seeds 31 and 47; kept `panel` 47 (`frame.png`, the HUD's panels, 9-sliced at 20 px) and
`card` 31 (`card.png`, the level and treasure cards, 19 px), each scaled down, near-black turned to
a dark brown and the inside filled flat so the border is thin (the old card's border covered its
text). Fight backdrops (`fight/<kind>.jpg`): the dread style, eye-level, empty of people, an open
floor in front; seeds 5 and 9, kept centre 9, hall 9, hub 9, room 5 (room 9 drew a woman), darkened
to 70%. Wallpaper (`wall.jpg`): "seamless tileable wallpaper ... damask ... dark jade green and dull
tarnished gold", seed 3 of 3 and 7, 512 px.

### The ten ghosts (planned, 2026-11-21: to make when ComfyUI is reachable)

One full figure each, plain light background (cut out like the characters), 832x1216, SDXL base,
the dread style: "full body figure of <ghost>, 1960s Vietnamese horror poster gouache, muted jade
and dark red, pale glowing skin, standing, plain background". Save each as
`art/sprites/<id>/body_calm.png` (and `face_calm.png`, its head), then set its `sprite` in
`assets/match/monsters.csv` to `<id>`.

| id | ghost |
|---|---|
| moths | a swarm of pale moths shaped like a hunched woman |
| rats | a pack of grey rats wearing a torn usher's cap |
| stagehand | a hanged stagehand, rope at the neck, work clothes, feet off the ground |
| prompter | a thin man in a prompter's hood holding a script, mouth sewn shut |
| usher | an old usher in a red uniform with a lantern, eyes white |
| usherette | a young usherette in a 1940s uniform with a tray of tickets, burnt edges |
| ghost-chorus | three singers in white ao dai fused at the shoulders |
| the-spider | a great spider with a porcelain opera mask for a face |
| pianist | a pit pianist in a charred tailcoat, fingers too long |
| ticket-girl | a box office girl behind a floating ticket window, gold coins in her hands |

### The twelve zodiac ghosts (2026-11-22)

The user asked for the ghosts as the twelve Vietnamese zodiac animals (Ty, Suu, Dan, Mao, Thin, Ty,
Ngo, Mui, Than, Dau, Tuat, Hoi: rat, buffalo, tiger, cat, dragon, snake, horse, goat, monkey,
rooster, dog, pig), ghost monsters of the house. `sprites/<animal>/` holds `body_calm.png` (768 px
tall, facing right) and `face_calm.png` (512 px); the other faces fall back to calm.

1. Text to image, SDXL base, 1024 x 1024 (tiger and pig 1216 x 832), 30 steps, cfg 7, dpmpp_2m
   karras. Prompts and style in `tools/art/lastcurtain_zodiac.json`: "(the ghost of a <animal>:1.2),
   <subject>, (ghost:1.4), (spectral apparition:1.3), (translucent glowing pale bone-white
   body:1.3), ... (hollow empty black eye sockets:1.3) ..." + the oil pastel words.
   The characters' style alone (seeds x01-x02) drew plain animal studies; the weighted ghost words fixed it.
2. Picks (ComfyUI `output/lastcurtain/zodiac2/`): rat 112, buffalo 212, tiger 322, cat 412,
   dragon 514, snake 612, horse 712, goat 811, monkey 911, rooster 1022, dog 1111, pig 1222.
   Turned down: tiger 311-314 and dragon 512 (head close-ups; "small in the frame, seen from far
   away" and a landscape page fixed the tiger), dragon 511 (a winged western dragon), snake 611
   (legs), pig 1211 and rooster 1011-1012 (plain farm animals), pig 1221 (not a pig).
3. Cut-out: birefnet (`RemoveBackground`) mask saved as an image, then
   `python3 tools/art/pack_ghost.py <picture> <mask> <animal> <this folder> <head x> <head y> [mirror]`
   (head point and mirror per ghost: rat .85 .4 m, buffalo .85 .3 m, tiger .9 .3 m, cat .8 .15,
   dragon .75 .12, snake .8 .1 m, horse .85 .2, goat .82 .22 m, monkey .5 .15, rooster .85 .05 m,
   dog .95 .25 m, pig 1 .35 m).

Not yet tied to the match: which monster or café guest each one becomes is the user's call.

## The campaign's characters (2026-11-23, overnight; art list `docs/design/art-requests-2026-11-22.md`)

ComfyUI on the user's PC, SDXL base 1.0, 30 steps, cfg 7, dpmpp_2m karras. Prompts:
`tools/art/lastcurtain_characters.json`. Outputs in ComfyUI's `output/lastcurtain/` (`slice1/`,
`slice2/`, `zodiac/`, `lethanh/`, `portrait/`, `expr/`, `mask/`, `work/`).

**How a figure is made** (the "same person" rule and the pose fix from the concept art):
1. **Body:** img2img at 0.75-0.8 from a pose base: an approved concept figure, mirrored so it turns
   right (men: `ghost_attacker_12`; women: `hero_datdo_12`; the boy: `ti_12`). Text to image gave
   front views, crops and a different style every seed; the base keeps the stance, scale and paper.
2. **Cut-out:** birefnet mask (`mask/`), then `tools/art/prep_figure.py`: the figure, and a
   portrait box (head and shoulders, 512:627, the head about half the width) scaled to 1024 x 1256.
3. **Portrait:** img2img 0.3 over that crop (`portrait/`), then 7 faces with ExpressionEditor
   (`soft` values of `tools/art/lastcurtain_expressions.json`, crop 1.7), each cut out (`expr/`).
4. **Pack:** `python3 tools/art/pack_figure.py <cut body> <id> <this folder> [faces dir]`:
   `body_calm.png` (768 px tall), `face_<face>.png` (512 x 627; without portraits `face_calm` is
   cut from the body) and `anchors.json` (shoulders and wrists, **estimated** from the outline).
   Only `body_calm` is painted; other body faces fall back to calm.
5. **Bound form:** the khăn tang painted (`tools/art/paint_khantang.py`: band and eye hollows, mask = band + 10
   px) and inpainted at 0.55 (`ghost_bound_2` recipe); the freed mask is reused.

### Slice 1 (art list section 0)

| Sprite | From | Notes |
|---|---|---|
| `player_male` | `slice1/player_male_103` (man base, 0.75) | Dark jacket, white T-shirt; trousers came out khaki, not jeans. **No fan:** two inpaints at the hand (351, 352) drew a pocket. |
| `player_female` | `slice1/player_female_h_201` (woman base, 0.75) | Same outfit; from the woman base, not the man (the same stance came out either way). No fan. |
| `lethanh` | concept `lethanh_b_21` | The hostess; `face_happy` is `portrait_kind`. Extra: `portrait_uneasy.png`, `portrait_cold.png` (ExpressionEditor; the revealed portrait still needs its strings). |
| `ti`, `ti_bound` | concept `ti_12`, `ti_bound_2` | Front view (approved concept). **The hat stays on his head:** inpainting it onto his back failed 4 ways (hat redrawn, seams, a cartoon head). |
| `zodiac_rat` | concept `rat_b_33` | `pack_ghost.py`, head .6 .12. **No props:** img2img 0.6 (41, 42) and text (s1_rat_71) lost the ghost or added a robe, never the spectacles. |
| `ghost_attacker` | concept `ghost_bound_2` | Bound only (ordinary ghosts have one painting). |
| `hero_datdo` | concept `hero_datdo_12`, mirrored | With portrait and 7 faces (warm). |
| `backdrops/foyer_today.jpg` | `s1_foyer_today_72` | 1344 x 768, text to image. |
| `backdrops/portrait_corridor.jpg` | `s1_portrait_corridor_71` | 72 had a realistic man's portrait on the wall. |
| `backdrops/box_office.jpg` | concept `box_office_12` | |
| `cafe/back_wall.jpg` | `slice1/cafe_back_wall_ext_92` | 2480 x 640. The plain outpaint (81, 82) drew green panels and seams; instead the centre (paper margin cut off) got mirrored sides, inpainted at 0.62. Not yet wired into `CafeRoom.cpp` (a code task). |

### Slice 2: all the characters (art list sections 1b-7)

Picks (ComfyUI `slice2/<id>_freed_<seed>`, bound `bound/<id>_bound_<seed>`); every recruit and
boon giver has `<sprite>` (freed: body, 7 faces) and `<sprite>_bound` (body, `face_calm` cut at
the same portrait box). Bound inpaint 0.55 removed the cloth on Quang and the girl with the hoe;
0.42-0.48 kept it, so later ones used 0.44-0.5.

| Sprite | Pick (freed, bound) | Notes |
|---|---|---|
| `nhan` | 2, 2 | Shirt and jacket, sandals; no banner scarf or satchel. |
| `diep` | 1, 2 | Glasses, green shirt, medical bag; no armband or notebook. |
| `quang` | 1, 4 | Helmet without leaves, rifle and pack. |
| `tai` | 4 (0.85), 3 | Seeds 1-2 drew an old man in a coat. White shirt; no tape or shears. |
| `tulanh` | 4 (0.85), 3 | Checked scarf, black trousers, white blouse (not black áo bà ba); no rifle. |
| `phong` | 4 (0.82), 3 | Seeds 1-2 came out old; "young man of 28" weighted. |
| `khoi` | 2, 2 | Wire coil at his feet. |
| `hien` | 2, 2 | Violin and bow; head box set by hand (`prep_figure.py ... 372 115`). |
| `bay` | 2, 2 | Pale pith helmet on, walking stick; a collar tab is painted (minor). |
| `boon_ferry_mother` | 2, 2 | Conical hat and oar. Icon `ui/boons/boon_ferry_mother.png`. |
| `boon_girl_with_hoe` | 2, 3 | **No hoe** in either seed. Icon drawn. |
| `boon_wife_four_days` | 2, 2 | Pale pink áo dài on a pink ground (cut out); no pillow. Icon drawn. |
| `cafe_bao`, `cafe_mai` | 2, 2 | Freed only. Bảo wears a coat and tie, not a doctor's coat; **neither has the loose khăn tang at the neck** yet. Mai holds a bowl. |
| `hero_electrician` | 1 | Standing (a board figure), not seated at a table; no tool pouch. |
| `ghost_defender`, `_supporter`, `_speedster`, `_allrounder` | `slice1/ghost_<id>_1`, bound 3 | Bound only, like `ghost_attacker`. |
| `zodiac_<animal>` | text to image, `zodiac/<animal>_<seed>` | rooster 13, cat 11, buffalo 13 m, dog 13 m, goat 12, pig 11 m, monkey 13 m, horse 12, snake 11 m, tiger 13 m, dragon 13 (m = mirrored); `pack_ghost.py` head points in the commit history. **No props came through** on any of them (the list says props are a guide); most read as pale animals rather than ghosts, and the engine's red glow and strings carry the menace. The portraits are `face_calm` crops, not separate paintings. |
| `backdrops/dragon_smoke.jpg` | `zodiac/dragon_smoke_21` | A crouching smoke dragon with red threads; not coiled above the stage. |
| `lethanh` extras | | `portrait_revealed.png` is `portrait_cold` with red strings drawn in code. |
| `boon_*` icons | `tools/art/draw_boon_icons.py` | Drawn glyphs, like the slingshot. |

**Le Thanh's other forms** (img2img from the hostess; 0.7 kept her pose, so 0.85-0.88):
`lethanh_dance` (`lethanh/dance_4`, a hand over her head), `lethanh_conductor` (`conductor_3`: one
arm raised, not both), `lethanh_ember` (`ember_bride_3` from the concept `ember_b_41`, mirrored:
an orange dress in smoke, still not embers; its `face_calm` is the ember portrait). At this
denoise the face drifts from the hostess: **provisional**, worth a better method (a pose control
model, if the user installs one).

**Not made:** the player's `fan_open` pictures and the fan itself (inpainting a closed fan into
the hand failed twice).
