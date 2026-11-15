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
