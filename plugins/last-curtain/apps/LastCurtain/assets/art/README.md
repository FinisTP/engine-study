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
scheming, afraid and talk, all with alpha. Huy is the template; the other guests use his sprite
(tinted) until the motion is proven in the game.

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

### The specter (2026-11-18)

`sprites/specter/`: the user asked for an eldritch specter whose face is only a mask making eerie
expressions, in the same oil pastel, then for it to look scarier and more intimidating (option E of
five, the user). How it was made:

1. A first specter (text to image, seed 7601, a gaunt robed figure) got a painted mask and an img2img
   at denoise 0.45 (seed 1). SDXL drew a bird-skull beak every time; the mask had to be painted on.
2. Escalated: img2img of that picture at denoise 0.62 (seed 13) with "a towering gaunt eldritch
   specter ... impossibly long spindly arms with too many long jointed fingers like spider legs, a
   ragged black theatre robe ..., the robe hem dissolving into dripping black tendrils and smoke, black
   tendrils rising from its shoulders" + the style + "eldritch horror, overwhelming dread": a faceless
   mass of hair with spider legs rising from its back.
3. Mask again: a rough bone-white oval, two black eye holes and a thin red mouth painted over its head
   at (397-473, 172-290), then img2img at denoise 0.48 (seed 22) with "a cracked white porcelain theatre
   mask with empty black eye holes and a thin red painted mouth hangs where its face should be, framed
   by a mass of long dripping black hair and strands, long black spider legs and spindly jointed limbs
   rising from its back". (A "ribcage" prompt turned every try into a skeleton.)
4. Faces: the `specter` values in `tools/art/lastcurtain_expressions.json` (scheming gentler: roll -9,
   yaw -4, or the tilt drags background paper in); the body scaled 2.5x; the portrait a 320 x 320 crop
   at (270, 70) scaled to 1024.
5. The head moves change birefnet's outline, so each body frame keeps the calm body and takes only a
   feathered region around the head (850-1300, 320-890 at 2.5x) from its changed frame; the bottom
   90 px (the paper's edge) are cut away.

`stage/stage.jpg`: the duel's backdrop, seed 7121, "an empty old opera house stage seen from the
stalls, heavy faded red velvet curtains ..., a single cold pool of light ..." + the style.
