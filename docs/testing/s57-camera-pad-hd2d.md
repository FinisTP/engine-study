# S57: battle fixes, the Octopath battle view, HD-2D polish, the gamepad (2026-11-25 overnight)

The user's notes of 2026-11-25 03:08 and their answers: the battle view is **Octopath** (one fixed
wide shot, the striker steps in and back, a short zoom punch on each blow, no panning); music: I
choose a candidate per fight; pad glyphs: clean shapes drawn in code; the PC's GPU may run overnight
jobs. Plans: `docs/design/plans/2026-11-25-hd2d-polish.md`, `2026-11-25-gamepad.md`.

## 1. What changed

| Commit | What |
|---|---|
| `#UMBRA-445` | No empty box beside the actor while aiming (the dock is drawn only with rows or a title). A foe that never rises stays on screen after its fall is told: down, then its dissolve 0.5 s later, then gone (it used to vanish before the last blow landed). Nothing is told while the battle view eases in or out (a board ability no longer starts before the view has arrived). The battle view: one wide shot aimed at the stage, no close-ups, no turn; the striker steps 1 tile in and back over 1.4 s; a move's push-in is 40% of the board's (`battle.punch`). HD-2D: the engine's tilt-shift (`PostSettings::dof`, `Post.md` 6c; light, 0.004 of the screen), its sharp band at the acting figure; the diorama (plinth, gold lit edge, black velvet); a lamp per room; a dark outline on every sprite (`sprite.surface.hlsl`). |
| `#UMBRA-446` | The pad: `PadL1/R1/L3/R3`, triggers as axes 4 and 5, `Input::PadLast`, `platform::Rumble`. Board: Square opens Kỹ năng, L1/R1 step the camera along the turn order, the right stick pans, L2/R2 zoom; Triangle ends the turn, Circle goes back (as before). Café: L1/R1 switch who walks, Create opens the notebook, L1/R1 turn its tabs. Map: the right stick pans, L2/R2 zoom. Every screen: the left stick moves the focus like the d-pad; the right stick scrolls the top screen's scroll boxes (`UiInput::padScroll`). Prompts: a legend of PlayStation glyphs (`art/ui/pad`, drawn by `tools/art/pad_glyphs.py`) at the bottom right while the pad was used last; the café hint has a pad wording. Settings: Rung tay cầm Bật/Tắt; the pad shakes on a hit and on a fall. |
| `#UMBRA-447` | The battle floor's reflection (15%); records. |
| `#UMBRA-448` | A faint light shaft under each lamp. |
| `#UMBRA-449` | While a foe thinks on the board, the camera sways 1.5° around its target (`hd2d.drift`, `TableCamera::SetDrift`); still on your turn and in a battle. |

## 2. Cloud checks

- `UmbraUiTests` (21 cases), `UmbraCoreTests` (44, the post tests), `UmbraCampaignTests` (62, the
  localization checks), `UmbraCafeTests` pass. No new rule tests (the user, 2026-11-25 02:11).
- Captures (SwiftShader): T1's board with the diorama edge, the outline and the tilt-shift; a
  faction match's battle view with the striker at about 25%; the pad legend (`ui-preview`).
- Not run: anything on Windows, any pad, rumble, frame time.

## 3. Not done tonight (and why)

- HD-2D task 3 is only half there: each lamp has a faint shaft (one soft particle stretched to the
  floor, `hd2d.shaft`, off with Giảm); a proper cone with a painted texture needs a soft-alpha
  surface the renderer does not have yet. The dust near lamps is unchanged.
- Gamepad task 7 (a prompts test): skipped with the rule-test pause; only two hints name keys.
- The battle picks keep their mouse layout; the d-pad moves through them as through any menu.

## 4. Windows check (please)

1. T1: no empty box beside the Rat while aiming. Kill the Rat: its last blow lands, it falls, then
   dissolves.
2. A battle: one wide shot with both sides in; the striker steps in and back; a small zoom on the
   blow; nothing starts until the view has arrived. A board ability waits for the view too.
3. The board: on their turn the camera sways a little (tell me if it is too much); the plinth's gold edge and the dark around it; top and bottom of the screen a little
   soft; figures have a thin dark outline. Frame time with and without (`Umbra --benchmark` or the
   overlay) on your laptop.
4. With the PlayStation pad only: the legend appears at the bottom right once a pad button is
   pressed and goes when the mouse moves. Cross picks, Circle backs, Triangle ends the turn, Square
   opens Kỹ năng, L1/R1 look along the order, the right stick pans, L2/R2 zoom. In the café: the
   stick walks, Cross talks, L1/R1 switch, Create opens the notebook, L1/R1 its tabs, the right stick
   scrolls its columns. The map: right stick and L2/R2. Settings: turn rumble off and on.
5. Music: see §5.

## 5. Music

Five candidates were made on the PC (`Temp\music-candidates\audio\music\cand_a`..`e.ogg`,
78 s each; ffmpeg was not on the PC's PATH, so the cuts were made in the cloud). Measured: bass
(below 150 Hz) against the melody band (400-3000 Hz), and when the melody band first rises:

| Candidate | Style asked | Bass vs melody | Melody from |
|---|---|---|---|
| a | violin and brass, marching snare | +11.0 dB | 0 s |
| b | flute and oboe over fast strings | +9.0 dB | 0 s |
| c | piano and cello, haunted opera house | -4.4 dB | 1 s |
| d | Vietnamese bamboo flute and zither over strings | -7.1 dB | 0 s |
| e | dark waltz, violin and accordion | +1.2 dB | 0 s |

a and b are bass-heavy (the complaint about the last take), so they are out. Chosen:
**tutorial = c**, **skirmish = d**, **zodiac (boss) = e**. Each is `fight_<kind>_battle.ogg` (the
take, bass -3 dB, melody band +2 dB, loudness -16 LUFS) and `fight_<kind>.ogg` (the same, rolled off
above 1.8 kHz and 4 dB quieter, for the board), the same length so the swap keeps its moment. To
swap one, cut another candidate the same way (`tools\audio\battle-music.bat` has the filters). Listen
to all five and tell me what to change.
