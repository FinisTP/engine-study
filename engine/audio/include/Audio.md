# Audio

Small on purpose: load short WAV or Ogg clips, stream Ogg music, add it all together, hand the
result to the sound card.
`Umbra::Audio` has no SDL dependency (it is tested headless). The SDL device lives in
`engine/platform` (`AudioDevice`) and only pulls mixed samples from the `Mixer`.

```
SoundBank (files -> AudioClip, cached)  --shared_ptr-->  Mixer::Play  (game thread)
                                                          Mixer::Mix   (SDL audio thread) -> device
```

## Supported files

Every sound runs at 48000 Hz (`mixRate`), so the mixer never resamples. Two containers:

- **WAV**, 16-bit PCM (below): short sounds, nothing to decode.
- **Ogg Vorbis** (`.ogg`, decoded by stb_vorbis): music, and optionally short sounds. 48000 Hz,
  mono or stereo; other rates are refused with the reason. A short `.ogg` is decoded at once by
  `SoundBank::Get`; music (`SoundBank::GetMusic`) stays compressed in memory (a 3-minute loop
  is ~3 MB instead of ~70 MB of float samples) and is decoded while it plays.

### WAV

| Field | Required value |
|---|---|
| Container | RIFF/WAVE |
| Encoding | PCM (format tag 1), 16-bit signed little-endian |
| Channels | 1 (mono) or 2 (stereo, interleaved L R) |
| Sample rate | 48000 Hz (`mixRate`) |

Anything else is rejected with a reason ("sample rate is 44100 Hz; convert it to 48000 Hz").
One fixed rate means the mixer never resamples, which keeps the code short. Convert assets
with any audio tool (e.g. `ffmpeg -i in.ogg -ar 48000 -ac 1 -c:a pcm_s16le out.wav`).

A WAV file is a list of chunks: 4-byte id, 4-byte little-endian size, data, one pad byte if
the size is odd. The parser walks them, keeps `fmt ` and `data`, ignores the rest (e.g. `LIST`),
and refuses any chunk whose size runs past the end of the file.

A 16-bit sample `s` (-32768..32767) becomes `s / 32768`, a float in -1..1.

## Mixing

Sound waves add: two sounds playing at once is `a[i] + b[i]` per sample. For each output frame:

```
left  = sum over voices of  sampleL * volume
right = sum over voices of  sampleR * volume     (mono clips use the same sample for both)
then clamp each to -1..1
```

The clamp matters: several loud clips can sum past 1, and converting that to 16-bit would wrap
around into loud clicks. Clamping just flattens the peak (mild distortion, no clicks).

## Music

`Mixer::PlayMusic(track, volume, fadeSeconds)` plays one looping track. Each playing track is a
stream: a `MusicDecoder` (stb_vorbis reading from the track's bytes) and a gain that moves toward a
target by a fixed step per frame:

```
step = start gain / (fadeSeconds * 48000)        (frames to fade over)
per block of n frames: gain moves by step * n toward its target, ramped sample by sample
```

A new track starts at gain 0 with target `volume` while every older stream gets target 0: a
crossfade. A stream that has faded to 0 is dropped. `PlayMusic(nullptr)` fades everything out;
asking for the track that is already playing only changes its volume. At the end of the file the
decoder seeks back to the start, so a track loops; AI-made music is made to loop without a jump
(`plugins/ai-authoring/modules/localai/include/SoundGen.md`). Music keeps playing while the game is
paused and is not stopped by `StopAll` (that is for sound effects).

The game picks the track from the live scene's **Music** component (third-person plugin): when
the scene changes (portal, arena, restart) it crossfades over 1.5 s.

## Lifetimes and threads

- A voice holds a `shared_ptr` to its clip, so `SoundBank::Clear()` (scene change) is safe
  even while a sound is still playing; the samples are freed when the voice finishes.
- `Mix` removes finished voices, so `VoiceCount()` returns to 0 once everything has played.
- At most 32 voices; `Play` returns false beyond that (footstep spam can't grow memory).
- One mutex guards the voice list. `Play` and `Mix` hold it only briefly, so the audio
  thread never waits long.

## Pausing

Voices are on a bus: `Game` (footsteps, interactions) or `Ui` (menu clicks).
`SetGamePaused(true)` freezes game voices where they are (they resume from the same frame)
while UI voices keep playing.

## Missing sounds

`SoundBank::Get` returns `nullptr` for a missing or unsupported file and records one
`sound.load` warning; later requests for the same path return `nullptr` silently.
The scene validator also warns (`entity.interactable.sound`) when a scene refers to a sound
file that does not exist, so `UmbraSceneCheck` catches it before the game runs.
