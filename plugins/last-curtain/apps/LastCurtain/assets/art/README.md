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
