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

## Stages

`assets/campaign/stages/<id>.json`, `"format": "umbra.lc-stage"`, `"version": 1`:

| Field | Meaning |
|---|---|
| `kind` | `story` (needs a unit with the `boss` flag) or `skirmish` |
| `title`, `titleEn`, `level` (recommended), `partyCap`, `backdrop`, `music`, `xp` | |
| `map` | equal rows: `#` wall, `.` floor, `~` hazard, `W` window, `R` rope post, `S` party start (at least `partyCap`), `D` doorway between rooms |
| `units` | `who` (cast id), `at` [x, y], `side`, `level`, `flags` (boss, recruit, strung), `height`, `behaviour` {`kind`: none, aggressive + `after`, passive, reinforcement + `round`}, `drops` [{`item`, `percent`}] |
| `items` | `id` (take-once, kept in the save), `item` (an `items.csv` id), `at` |
| `mustFree`, `noRevive` (sides that never revive), `gimmicks`, `aims` {`start`, `recap`} | |
| `scenes` | moment → scene id: `start`, `end`, `round:<n>`, `firstAttacked:<cast id>`, `falls:<cast id>`, `freed:<cast id>` |
| `twists` | skirmishes: one per refill, each `name`, `units`, `gimmicks` |

`LoadStage` checks each and names the file, the field and the fix.

## Building a fight

`BuildStage(def, cast, growth, stage, party, refill, taken, seed)`: the map becomes the match's board
(every square but `#` open, doors between open neighbours); the party, up to `partyCap`, stands on the
`S` squares in reading order, the first one flagged as the player and carrying its slots; then the
stage's units and, for `refill` ≥ 1, that refill's twist (the last twist repeats); reinforcements wait off
the board in `MatchState::arrivals`; items whose id is in `taken` stay away; then `BeginMatch`. It
returns each unit's cast id and drawn height beside the state.

## Gimmicks

A stage's `gimmicks` object; a twist's `gimmicks` replace the stage's keys of the same name.

| key | fields | becomes |
|---|---|---|
| `windows` | `spawn` (cast id), `level`, `max`, `pattern` (lists of window numbers per round), `drops` (`item`, `percent`) | the map's `W` squares in reading order, numbered from 0 |
| `hazards` | `count`, `damage` | random marked squares each round |
| `breath` | `unit` (one of the stage's units), `damage`, `every` | a marked row, shown a round ahead |
| `dark` | `sight` | how far a passive enemy sees (default 4) |

`Sync(BuiltStage&)` gives spawned units a name, height and plain behaviour; call it after each action.

## Scenes inside a fight

`StageMoments(def, stage, told, moments)` returns the scene ids whose moments the fight has reached,
each once per attempt (a fresh `Moments` for each try): `start`, `round:<n>`, then from the told log
lines `firstAttacked:<id>` (a blow), `falls:<id>`, `freed:<id>`, then `end` (won) or `lost` once all is told. It only
reads. The stage screen calls it when `MatchPlay::Quiet()` (no battle on the table, every line told),
pauses the fight (`Pause`), plays the scenes on the dialog screen and resumes.

## The enemy brain

`EnemyAction(def, built, state, level)`: an **aggressive** unit holds (ends its turn) until round
`after`; a **passive** one holds until it is hurt or a party unit stands in its room within 4 squares
(rooms from the map, doorways `D` between them); otherwise the faction bots (`ChooseAction`) play it.
The app gives it to `MatchPlay::SetBrain` for a stage. Reinforcements are rules in the match
(`Match.md` "Arrivals").

## The soak (`CampaignSoak.hpp`, `UmbraTool campaign-soak <stage> [--seeds 200] [--refill n]`)

Bots play both sides: the party with `PartyAction` (Medium), enemies with `EnemyAction`. The party is the
player, Bảo and Bà Mai up to the cap, at the stage's level **plus the refill number** (a refill comes after
a story stage). A run stops at round 40 (bots can circle a 1-HP foe for ever) and counts as unfinished.
It reports wins, trapped losses, other losses, unfinished, average rounds, minutes (6 s per party turn and
1 s per told line: a guess until the user's play times), how often the player fell, falls by cast id and
items found. `assets/campaign/balance.json` holds the win bands (and T1's player falls); trapped losses and
minutes are reported only: the bot never hits the Rat while Tí is strung and plays fast, so both need a
person (the user's check). The `[campaign-balance]` test (ctest `Umbra.Campaign.Balance`, `build soak`)
checks every stage and refill on 200 seeds.

Tuning record (2026-11-24, 200 seeds from seed 1):

| Stage | Wins | Notes |
|---|---|---|
| t1 | 100% | player never fell; about 5 rounds |
| t3 | 78% | the Keeper's breath every 2 rounds, 6 damage |
| stage1 | 45% (trapped 11%) | the Rat at level 2 (level 3 gave 24-42%); windows one at a time, every third round, level-1 rat specters; Gừng in about 29% of runs, the keepsake in 24% |
| sk_props 0/1/2 | 76 / 76 / 88% | 4 ghosts (levels 2-3); hazards 2 a round, 4 damage |
| sk_stairs 0/1/2 | 74 / 78 / 86% | 4 ghosts; "Tắt đèn" adds one passive level-1 specter (two level-2 bosses dropped wins to 12%) |
| sk_boxoffice 0/1/2 | 85 / 88 / 87% | thief level 4, two ghosts; refill 2 adds a level-5 thief and ghost |
