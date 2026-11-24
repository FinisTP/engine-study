# The Last Curtain — sound design

Status 2026-11-24: music for six places and the typing sound are in (`#UMBRA-411`); the rest below is the plan.
Every cue is a row in `plugins/last-curtain/apps/LastCurtain/assets/sounds.csv`; the files and how they were made are in
`assets/audio/README.md`. Sounds are made on the user's PC (`tools/audio/campaign-audio.bat`, `UmbraTool sound`).

## 1. Layers

| Layer | What | Rule |
|---|---|---|
| Music | one loop per place: title, cafe, map (also who fights), fight, result, gameover | `music:<place>` rows; a place change crossfades over 1.5 s; a place with no row fades to silence |
| Room tone | a quiet bed under the music (rain on the café roof, the empty hall's hum) | planned: `tone:<place>`, looped, about -30 dB, never stops between screens of the same place |
| Cues | one-shot events: click, type, hit, death, revive, door, item | short WAVs, 48 kHz mono, -1 dB peak, faded tails |
| Stingers | two-to-four-second phrases over the music: level up, item found, a room opens, a person joins | planned; the music ducks under them |

## 2. Levels and fades

- Mean loudness of the music is evened to about -18 dB by the `sounds.csv` volume column (`audio/README.md`).
- Players set music and effects separately (Settings → music, effects; 0 to 100).
- Dialogue: the typing key every 0.07 s while a line types, at 0.35; the music stays.
- Planned: duck the music by 6 dB under a stinger and under a model's spoken line, if voices come.

## 3. What each moment should sound like

| Moment | Now | Planned |
|---|---|---|
| Title, new game | slow piano and strings | — |
| Café | warm piano and đàn bầu | rain tone; a cup set down when an order is served |
| Map, who fights | sparse piano over a cello drone | a paper rustle when the map is dragged; a soft pin when a ticket is picked |
| Fight | low strings and soft drums | a different loop for a boss room; the night's hit, death and revive cues per ability kind (blow, heal, guard) |
| Result | a short warm rise | a level-up stinger per person who grew |
| Game over | dark cello and piano | silence for a beat before it starts |

## 4. Open questions for the user

1. Takes were kept without listening; the second take of each is beside it on the PC. Listen and swap any that is wrong.
2. Should the fight music change when one side is close to losing (a second, tenser layer)?
3. Voices: none today. A model voice would need a local TTS model and its own consent and licence check.

## 5. Checks

- `ArtTests` "Every sound cue in sounds.csv names a WAV the engine plays": every row's file exists and decodes
  (WAV for cues, Ogg Vorbis for `music:` rows).
- On the PC: hear each place's music start and crossfade, the typing under a dialogue line, and the music and
  effects sliders (`docs/testing/s53-campaign-feedback-2.md` §4).
