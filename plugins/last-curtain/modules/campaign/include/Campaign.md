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

## Playing a scene

`ScenePlayer` plays one scene with no drawing; the app (`DialogView`) reads it each frame.

1. `Update(seconds, lettersPerSecond)` reveals the line letter by letter. Letters are code points,
   so a Vietnamese letter such as "ử" is never cut in half.
2. `Advance()` (a click or Space) completes the line; once complete it goes on (the line's `jump`,
   else the next line). It never passes a line with choices.
3. `Skip()` runs to the end but stops at the first line with choices, complete, choices shown.
4. `Choose(i)` sets the choice's fact in the `Facts` map and jumps.
5. `Left()` and `Right()` give the two places a side (0 near the middle); the speaker is `lit`.
   Narration lights nobody. Someone brought into a taken place sends its owner out.
6. `Log()` holds (name, text) of every line started; `TakeSound()` gives a line's sound once.

`FillTokens` fills `{player}` (the typed name, or the cast's `nameless` row when empty), `{anh/chị}`
and `{Anh/Chị}` (`anh` for "male", `chị` otherwise).

## The prologue

`scenes/prologue.json` is the game's first minutes (draft for the user's review): today's night tour of
the restored opera house; the guide's torch dies; Lệ Thanh in a red bridal áo dài on the stairs already
knows the player's name; the house is "full tonight" in front of empty seats; she is gone and a red
string creaks down (into T1). Its one choice sets `lethanh.first` to `kind` or `wary`, which colours one
café line later (plan 6).

## Units

`assets/campaign/growth.csv` holds each class line by level (1–15): HP +2 a level (as the match's
`levels.hp`), Attack +1 at levels 4, 7, 10, 13, Defence +1 at 5, 9, 13, Speed +1 at 6 and 12; level 1 is
the match's `characters.csv` line. `cast.csv` adds `role` (the line), `hpPlus`, `attackPlus`, `movePlus`
(the character's own changes) and `kit` (ability ids joined by `;`; empty: the class's abilities up to
its level, from `jobs.csv`).

`MakeUnit(def, cast, growth, id, level, faction, enemy)` builds the fighting unit. An enemy is scaled by
its form: a **zodiac's attack and HP ×1.33**, an **ordinary ghost's ×0.6** (rounded). Allies never are.
At level 3: the Rat (attacker) 8 attack and 32 HP; an ordinary ghost attacker 4 attack and 14 HP; Tí
(speedster, HP −2, Move +2) 20 HP and 12 move.
