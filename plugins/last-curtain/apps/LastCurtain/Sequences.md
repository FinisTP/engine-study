# Sequences: how a told line plays out (the user, 2026-11-24 19:05, point 3)

When the fight tells a line (a blow, a heal, a fall), the screen can play it as a small timed scene:
the camera on whoever acts, an effect that spawns, flies and lands, the number rising when the hit
lands, a shake, a sound, a speech bubble. `assets/vfx/sequences.json` says which cue happens when.

The rules never read it. A fight plays the same with sequences, without them, or skipped; only what
the screen shows, and when, changes.

## The file

```json
{"format": "lastcurtain.sequences", "version": 1,
 "byKind": {"Blow": "blow"},
 "byAbility": {"na_cao_su": "na_cao_su"},
 "sequences": {
   "na_cao_su": {"length": 1.5, "cues": [
     {"at": 0.0, "do": "camera", "on": "user"},
     {"at": 0.1, "do": "bubble"},
     {"at": 0.3, "do": "projectile", "effect": "shot_trail", "on": "user", "to": "target", "seconds": 0.4},
     {"at": 0.7, "do": "hit", "on": "target"}]}}}
```

- A line takes its ability's sequence (`byAbility`, by ability id), else its log kind's (`byKind`, by
  `MatchPlay::LogName`), else it is told the plain way (as before sequences).
- `length`: seconds the line holds before the next one. Every use plays in full (the user's choice);
  holding the hurry key speeds the whole sequence and its effects up together.
- A cue: `at` (seconds), `do`, `on` (`user` = the one acting, `target` = the one reached).

| `do` | What happens |
|---|---|
| `camera` | the camera goes to `on` |
| `effect` | `effect` (assets/vfx/<name>.vfx.json) bursts at `on`'s middle |
| `projectile` | `effect` flies from `on` to `to` over `seconds` (eased in and out) |
| `hit` | the number rises and the cards change: before this cue, nobody sees the damage |
| `shake` | the camera jolts by `amount` (off when effects are set to Giảm) |
| `sound` | plays `sound` |
| `bubble` | the user's speech bubble (the ability's name) |

A sequence without a `hit` cue tells the hit when the line starts; without a `bubble` cue, the bubble too.

## Playing

`MatchPlay::Reveal` starts the sequence for the line it tells; `Update` fires every cue whose time has
come, in order. A line cut short (the next line, or skip) still tells its hit at once, never its effects:
skipping a turn shows the right numbers and spawns nothing. Effects come out of `TakeEffects()`; the app
(`MatchApp`) puts them where the figures are drawn (`MatchTable::Body`) through `Effects`.

## Effects

`Effects` loads every `assets/vfx/*.vfx.json` with its picture (engine `Particles.md` "Pictures"). A
picture that won't load leaves soft dots, said once in the log. At most 256 particles live at once.
Giảm (the settings) halves every effect's particles and turns off the shake.
