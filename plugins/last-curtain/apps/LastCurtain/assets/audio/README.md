# The night's sounds

One WAV per cue in `../sounds.csv` (48000 Hz, 16-bit, mono; `engine/audio/include/Audio.md`).
Made with Stable Audio 3 in ComfyUI on the user's PC; prompts and seeds below, rejects noted.

| Cue | Prompt | Seconds |
|---|---|---|
| step | a single soft footstep on an old wooden theatre floor, close, dry, no music | 1 |
| dice | a bone die rattling and tumbling across a wooden table, then settling, close, no music | 1.5 |
| door | an old heavy theatre door creaking open slowly in an empty hall, no music | 2 |
| item | picking up a small metal object from a wooden table, a short clink, no music | 1 |
| hit | a hard punch impact with a low thud, close, no music | 1 |
| death | a body falling heavily onto a wooden stage floor, a dull thump and a last breath, no music | 2 |
| bell | a single deep church bell strike at midnight, long ringing decay, distant hall reverb | 4 |
| turn | a soft low ominous chime, single note, short, theatre ambience, no music | 1.5 |
| click | a short dry wooden click of a button, very short, no music | 0.3 |
| killer | slow heavy footsteps dragging closer across an old wooden theatre floor in the dark, a low ominous rumble underneath, no music | 3.2 |

Made 2026-11-16: Stable Audio 3 medium base (`stable_audio_3_medium_base.safetensors` + `t5gemma_b_b_ul2`),
KSampler lcm, 50 steps, CFG 7, empty negative; two takes each (seeds 101 and 202), generated a
little longer than needed (the model's shortest is 1 s). Kept: step 202, click 101, dice 101,
door 202, item 101, hit 101, death 202, bell 101, turn 101, chosen by shape (the lower, warmer
take for a step, a fall and a chime; the longer rattle and creak; the brighter clink), not by
ear: listen and swap if one is wrong. Each was cut from just before its onset to its length
(0.25 to 4.5 s), faded out over 40 ms and set to a -1 dB peak.

Added 2026-11-16 for the battle royale: killer (same settings, 3.5 s takes; kept 101, which swells
as it comes closer; 202 rose and fell twice), cut to 3.2 s with a 300 ms fade, -1 dB peak.

Added 2026-11-17: pass ("a short bright rising chime of two clear bell notes, a small success,
theatrical, no music, no voice"; kept seed 101, which rises into its second note; cut to 0.9 s)
and fail ("a short low muted descending thud with a dull out-of-tune piano note, a small failure,
no music, no voice"; kept seed 202, the lowest and longest; cut to 1.1 s), 1.6 s takes, 200 ms
fade, -1 dB peak.
