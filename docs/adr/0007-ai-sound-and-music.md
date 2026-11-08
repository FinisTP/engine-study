# ADR 0007: AI music and sound effects, and music in the game (S4)

- Status: **accepted; implemented** (2026-11-08).
- Context: the user asked for AI-generated audio (settlement background music, battle music,
  attack sounds). The game played only short uncompressed WAV clips and had no music at all; the
  AI tools made pictures and 3D models through a local ComfyUI.

## Decisions (user, 2026-11-08)

| Choice | Decision |
|---|---|
| Models | ACE-Step 1.5 for music (Apache-2.0, ~4 GB VRAM) and Stable Audio 3 for effects (Community License, free below USD 1M revenue: warned), both native ComfyUI nodes |
| Format | Compressed Ogg Vorbis for music, WAV for effects |
| Wiring | A music system (scene music, battle music, crossfades) and RPG hooks (a skill's or item's sound), editable in the editor |
| AI UI design | No new AI tool: mockups come from SDXL or other tools; instead, test how well screens and their code can be written from a prompt (record in `docs/testing/s4-sound-and-menus.md`) |

## Design

1. **Generation is a tool feature** (ai-authoring plugin, never in the game): built-in ComfyUI
   graphs for both models, the same queue/wait/download code as pictures, then decode (dr_flac),
   resample to 48 kHz, loop or trim, normalize, encode (libvorbis for music). Provenance goes in
   `<name>.source.json` with the `models` list package-check already reads. Math in
   `SoundGen.md`.
2. **The game only decodes**: stb_vorbis (one public-domain file) in `engine/audio`. Music stays
   compressed and is decoded while it plays; a new track crossfades over the old one.
3. **No new scene field**: music is a data-only **Music** component from the third-person
   plugin (track + volume), so the editor's Add component, the validator and saving already
   handle it. Fights use the arena scene, so its Music component is the battle music.
4. **Fight sounds are content**: an optional `"sound"` on skills and items in the RPG content
   (rules ignore it; it is not part of the learned-AI fingerprint). A hit without one plays
   `audio/hit.wav`.

## Why Vorbis when ComfyUI cannot write it

ComfyUI saves FLAC, MP3 or Opus. The sound has to be decoded and changed anyway (loop crossfade,
trim, normalize), so the tool re-encodes it; with libvorbis on the tool side the game needs only
stb_vorbis to read it. MP3 (a single-header decoder exists, but an encoder is LGPL) and Opus
(libopus + opusfile in the game and the browser build) were heavier.

## Consequences

- vcpkg gains `libvorbis` (with libogg), used only by the editor and UmbraTool.
- Real-time hotkeys replaced the fixed Q potion key (S4 menus); saves gained an optional
  `hotkeys` list (older saves still load).
- The generated sounds are not judged here: the cloud workspace has no GPU or ComfyUI. Quality
  depends on prompts and models, like textures; the user checks it on their PC.
