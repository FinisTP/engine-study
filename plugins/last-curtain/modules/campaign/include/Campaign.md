# Campaign

The Last Curtain's campaign (design: `docs/design/2026-11-23-campaign-slice-1.md`). Headless: no
renderer, no window. Plans 3–6 fill it: the cast and scenes here; stages, the save, the theatre map
and the café later.

## The cast

`assets/campaign/cast.csv`: `id,name,sprite,bound,form,voice`. `sprite` is the folder under
`art/sprites/`; `bound` the sprite while strung; `form` one of `player`, `ally`, `strung`, `ghost`,
`host`, `zodiac` (how the screen tints them). The player's name is `{player}`; the app picks
`player_female` or `player_male` from the save.

## Scenes

`assets/campaign/scenes/<id>.json`, `"format": "umbra.lc-scene"`, `"version": 1`, a list of lines:

| Field | Meaning |
|---|---|
| `id` | the line's name, for jumps (may be empty if nothing jumps to it) |
| `speaker` | a cast id; empty = narration |
| `face` | calm, talk, happy, angry, afraid, scheming, neardead (`face_<face>.png`) |
| `text` | the words; `{player}`, `{anh/chị}`, `{Anh/Chị}` are filled in |
| `enter` | portraits brought in or moved before the line: `who`, `side` (left/right), `slot` (0 near the middle, 1 behind), `face` |
| `leave` | cast ids taken out before the line |
| `choices` | none or 2–4: `text`, `fact`, `value`, `jump` |
| `jump` | the next line's id; empty = the next line in the file |
| `sound` | a sound file, played if present |
| `model` | the model may reword this line (plan 6) |

A line runs: 1. `leave`, 2. `enter`, 3. the words. The loader checks, naming the file, the line and the
fix: unknown cast ids, unknown faces, more than two portraits a side, a speaker not on stage, one
choice or more than four, a jump to no id, an id used twice, and lines that jump in a loop with no
choice (the scene could never end). The stage is checked in file order; a jump that brings a speaker
who left is the writer's to avoid.
