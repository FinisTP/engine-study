# S4 — AI sound, music, turn menu and hotkeys: verification record

## Record

- Date: 2026-11-08
- Commit: `issue #UMBRA-77` (S4.1–S4.5), ADR 0007
- Tester: Claude (remote session), Linux cloud workspace (GCC 13, Clang 18, ASan/UBSan, Emscripten
  3.1.52, headless Chrome). No GPU, no ComfyUI, no sound card: generation runs against a stand-in
  ComfyUI, sounds are checked by their samples, screens through the CPU rasterizer.
- State: **implemented; verified headlessly.** Real ACE-Step / Stable Audio 3 runs, listening,
  and playing the new menus on Windows are for the user (§4).

## What was built

| Task | Where |
|---|---|
| S4.1 Turn menu | `fight` (Attack / Item / Flee) → `fight-moves` / `fight-items` → `fight-target`: an arrow over each fighter the move may hit (screen point = the scene camera's projection of a point above the head, `GameUi::ScreenPoint`); `back` steps back. Engine: list entries placed at bound points (`"at"`), widgets inside a focused entry light up with it |
| S4.2 Hotkeys | keys 1-4 hold a skill or an item (`UseHotkey`); `hotkeys` screen (H or pause menu); HUD hotkey bar in real-time fights; kept in saves (`"hotkeys"`, optional) |
| S4.3 Music in the game | stb_vorbis; `Mixer::PlayMusic` streams and crossfades; `SoundBank::GetMusic`; the **Music** component; `Main.cpp` switches on scene change; browser build too |
| S4.4 Fight sounds | `"sound"` on skills and items; `GameFlow::TakeSounds`; default `audio/hit.wav` (synthesized) |
| S4.5 AI sound | `AudioWorkflow` (ACE-Step 1.5, Stable Audio 3), `GenerateAudio`, `SoundAssistant` (decode, resample, loop, trim, normalize, WAV/Vorbis, provenance); editor **AI sound** window (generate, listen, keep, use as scene music or as a skill's/item's sound, undoable); `UmbraTool sound`; RPG forms Sound field |

## 1. Automated checks

| Check | Result |
|---|---|
| Linux GCC / Clang / GCC ASan+UBSan | **48/48 / 48/48 / 48/48** |
| Plugin sets (turn-only + learned AI; real-time only) | **37/37, 33/33** |
| WebAssembly suites (now with `Umbra.Audio`) | **15/15** |
| Browser build (`check-web.py`) | **13/13** |
| T80 `Umbra.Audio`: a 48 kHz `.ogg` decodes; 44.1 kHz and junk refused with the reason; music fades in from silence, keeps playing past the file's end (loop), crossfades to a new track, fades out to 0, survives `StopAll` | pass |
| T81 `Umbra.LocalAi [sound]`: both graphs and their sampler settings; model choice (ACE-Step all-in-one for music, Stable Audio 3 for effects, split ACE files refused); a run against the stand-in ComfyUI downloads its FLAC; an effect is trimmed to the sound (+5 ms / +60 ms), mono, peak -1 dBFS, saved as a WAV the game reads, with `.source.json` (`models` with license); a second save gets `_2`; music is resampled 44.1 → 48 kHz, its loop point jumps no more than a normal sample step, and the `.ogg` it writes plays in the game's decoder | pass |
| `Umbra.Tool.SoundLocalOnly` | a non-local ComfyUI address is refused (`ai.host`) |
| `Umbra.Game`: hotkeys saved; unknown skill on a key cleared; older saves have none | pass |
| `Umbra.Rpg`: `"sound"` on skills and items read, no unknown-field warning | pass |
| `Umbra.Ui` T79 additions: Attack → `attack`, a move → `move strike`, Esc → `back`, a click just above a target's point picks it (`target 2`), the hotkeys screen's first choice is key 1 | pass |

Pictures checked by eye with `UmbraTool ui-preview`: `fight`, `fight-moves`, `fight-items`,
`fight-target` (arrows over two enemies, the focused one gold with its name), `hotkeys`, the HUD
hotkey bar.

## 2. Prompt → screen and code: how efficient was it? (user request)

The user described two features in two sentences each; they were built from those words alone.

| Feature as asked | Screen files (data) | Engine C++ | Game C++ |
|---|---|---|---|
| "Turn menu divided into attack, item and flee; attack gives the attacking moves, then an arrow lets players select a viable target" | 4 screens, ~50 lines | ~35 lines: list entries at bound screen points; children take their focused entry's state | ~110 lines: menu steps and actions, moves/items/targets from the legal actions, a point over each fighter, screen projection (~15) |
| "Real-time pre-set hotkeys; a separate menu to assign attacks and items to hotkeys" | 1 new screen (~22 lines), HUD bar (~7), pause entry (1) | 0 | ~95 lines: default keys, use, labels, the assign actions; save field (~20, with migration rule); H key (4) |

What this shows:

- **Layout, look and navigation were pure data.** Menus, panels, lists, focus order, mouse,
  Esc-goes-back and dimming needed no code; previews could be checked in seconds without
  starting the game (`ui-preview`).
- **Code was needed where the screen needs facts the UI cannot know**: which actions are
  legal, where a 3D enemy is on screen, what goes in a save. Each new kind of fact is a small,
  testable C++ addition in the game, not in the UI.
- **Engine changes were rare and general**: one feature (placing entries at screen points)
  that any project can reuse for health bars over heads or quest markers.
- A data-only change could not have done the arrow: placing UI over 3D objects needs both the
  projection (game) and point placement (engine). After this slice, a similar request (e.g.
  damage numbers over enemies) is data plus one projected point.

## 3. Known limits

- No real model ran here; prompt quality, ACE-Step's handling of short loops, and how good the
  loop crossfade sounds on real music need the user's ears.
- Effects are mono (the game has no positional sound yet).
- The editor preview plays a sound once (no loop preview of music).
- Music in the browser starts after the first click (browsers block sound before it).

## 4. For the user (Windows)

| # | Do | Expect |
|---|---|---|
| 1 | Reconfigure (vcpkg adds libvorbis), build | Builds; all tests pass |
| 2 | ComfyUI: add `ace_step_1.5_turbo_aio.safetensors`; editor → AI sound → Music, "calm fantasy village, lute and flute", 2 variations → Generate | Two tracks; Play sounds right; the loop point is not audible when listening through the end |
| 3 | Keep one, open the settlement, Use as this scene's music, Save, Play | Music in the settlement |
| 4 | Same for battle music in `arena.scene.json`; start a fight | Crossfade to battle music and back after the fight |
| 5 | Add the Stable Audio 3 files; Sound effect "heavy sword slash" → Keep → For `skills/strike` → Use; fight | The new sound on every Strike hit; other hits play the default |
| 6 | Turn fight: Attack → a move → Left/Right/mouse on the arrows → Enter; Item; Esc steps back | Arrows over the enemies; Esc goes back one menu |
| 7 | H: assign a potion to key 3; real-time fight; press 3 | Hotkey bar shows it; the potion is used; the choice survives saving and loading |
