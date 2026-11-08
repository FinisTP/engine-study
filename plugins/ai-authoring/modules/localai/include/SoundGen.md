# AI music and sound effects (roadmap S4)

Music and sound effects come from a local ComfyUI, like AI textures and models: this computer
only, no cloud fallback. The editor's **AI sound** window and `UmbraTool sound` share this code.

```
idea + style -> ComfyUI (ACE-Step 1.5 or Stable Audio 3) -> FLAC -> 48 kHz -> loop / trim -> normalize
            -> music: audio/music/<name>.ogg      effect: audio/sfx/<name>.wav      (+ <name>.source.json)
```

## Models (user decision 2026-11-08)

| Use | Model | File (ComfyUI folder) | License |
|---|---|---|---|
| Music | ACE-Step 1.5 turbo | `ace_step_1.5_turbo_aio.safetensors` (`models/checkpoints`) | Apache-2.0 |
| Effects | Stable Audio 3 | `stable_audio_3_medium_base.safetensors` (`models/checkpoints`) + `t5gemma_b_b_ul2.safetensors` (`models/text_encoders`) | Stability AI Community License: free below USD 1M yearly revenue |

Both are native ComfyUI nodes (update ComfyUI if they are missing); the files are on Hugging Face
under Comfy-Org (`ace_step_1.5_ComfyUI_files`, `stable-audio-3`). ACE-Step needs about 4 GB of VRAM.
`PreferredSoundModel` picks the ACE-Step all-in-one file for music and a Stable Audio 3 file
(an "sfx" one first) for effects; either model can do the other job if it is the only one.
The license of the model used goes into the `.source.json`; `UmbraTool package-check` warns about
non-commercial ones like for textures and models.

## The graphs (`AudioWorkflow`)

Built in code, so nothing has to be exported from ComfyUI:

- **ACE-Step 1.5** (ComfyUI's own template): `CheckpointLoaderSimple` -> `ModelSamplingAuraFlow`
  (shift 3) -> `TextEncodeAceStepAudio1.5` (tags = the prompt, lyrics `[Instrumental]`, bpm, key,
  4/4, duration) -> `KSampler` (8 steps, CFG 1, euler) with a zeroed negative ->
  `VAEDecodeAudio` -> `PreviewAudio`.
- **Stable Audio 3**: `CheckpointLoaderSimple` + `CLIPLoader` (type `stable_audio`) ->
  `CLIPTextEncode` (prompt, empty negative) -> `EmptyLatentAudio` -> `KSampler` (lcm; a `base`
  checkpoint 50 steps at CFG 7, the distilled ones 8 at CFG 1) -> `VAEDecodeAudio` -> `PreviewAudio`.

`PreviewAudio` writes a temporary FLAC file, so nothing piles up in ComfyUI's output folder. The
queue / wait / download steps are the same as for pictures (`RunGraph`, `LocalAi.md`).

## Making a game sound from ComfyUI's output (`FinishSound`)

1. **Decode** the FLAC file (dr_flac) to float samples.
2. **Resample** to 48000 Hz, the one rate the mixer plays (Stable Audio 3 makes 44100 Hz).
   Each new sample sits at time `t = j * from / to` in the old samples; with `i = floor(t)`,
   `f = t - i` and the four old samples `p0..p3` around it (Catmull-Rom spline):

   ```
   out = p1 + 0.5 f (p2 - p0 + f (2 p0 - 5 p1 + 4 p2 - p3 + f (3 (p1 - p2) + p3 - p0)))
   ```

   It passes exactly through the old samples and is smooth between them, much cleaner than a
   straight line between two samples.
3. **Music: a seamless loop.** Generated music has an ending that doesn't lead back to its start,
   so a plain loop jumps. The last `F` frames (2 s, at most a quarter of the piece) are mixed into
   the first `F` frames and cut off:

   ```
   L = frames - F
   out[i] = s[i] * sin(w) + s[L + i] * cos(w)     for i < F, with w = (i / F) * pi/2
   out[i] = s[i]                                  for F <= i < L
   ```

   At `i = 0` the output is exactly `s[L]`, the sample that followed `s[L - 1]` in the original,
   so the end of the loop runs straight into its start. `sin^2 + cos^2 = 1` keeps the loudness
   even through the blend ("equal-power" crossfade) where a straight-line blend would dip.
4. **Effects: trim and shape.** Mixed to mono (`(L + R) / 2`; the game has no positional sound).
   Quiet start and end are cut where the level stays below 3% of the peak, keeping 5 ms before the
   sound and 60 ms after it so tails are not chopped. A 2 ms fade-in removes a click without
   softening the attack; a 30 ms fade-out ends it cleanly.
5. **Normalize**: every sample times `0.89 / peak`, so the loudest point is -1 dBFS
   (`20 log10(0.89) = -1`). A result whose peak is below 0.01 is called *almost silent* instead of
   being amplified into noise.

## Files

- Music is encoded as **Ogg Vorbis** (libvorbis, quality 0.4, about 128 kbit/s: a 3-minute loop
  is ~3 MB). The game keeps it compressed and decodes it while it plays (`engine/audio`, Audio.md).
- Effects are **16-bit 48 kHz mono WAV**, the format the game plays without decoding.
- `<name>.source.json` next to each: prompt, seed, seconds, kind, date and `models` (file,
  license, commercial) in the same shape as a generated model's provenance.
- Names come from the idea (`"Battle drums!"` -> `battle_drums`); a taken name gets `_2`, `_3`...

## The project's style

`assets/audio/style.json` (`{"music": "...", "effect": "..."}`) is added to every prompt, so all
tracks share a sound. Without one: "instrumental video game soundtrack, fantasy RPG, seamless
loop, no vocals" for music and "single isolated game sound effect, clean, no music, no background
noise" for effects.

## Using the result

- **Music** plays through a scene's **Music** component (third-person plugin): a track and a
  volume. The editor's AI sound window can put it on the open scene as one undoable edit. Fights
  happen in the arena scene, so the arena's music is the battle music.
- **Effects** play through a skill's or item's `"sound"` in the RPG content (a skill's when it
  hits, an item's when it is used; without one a hit plays `audio/hit.wav`).
