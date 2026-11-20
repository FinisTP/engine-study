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
