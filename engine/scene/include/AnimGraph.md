# Animation graphs (`animations/<name>.anim.json`, S8)

A graph says which clip a rigged character plays and when. An entity uses one through its
`animation` component; RPG actors through their `animation` field (enemies are spawned, not
placed). The math behind clips and skinning is in `engine/core/include/Skeleton.md`.

```json
{
  "format": "umbra.animgraph",
  "version": 1,
  "fade": 0.2,
  "states": [
    { "name": "dead",   "clip": "death", "loop": false, "when": { "flag": "dead" } },
    { "name": "hit",    "clip": "hit",    "on": "hit" },
    { "name": "attack", "clip": "attack", "on": "attack" },
    { "name": "run",    "clip": "run",  "when": { "param": "speed", "above": 2.6 }, "rate": { "param": "speed", "per": 4.5 } },
    { "name": "walk",   "clip": "walk", "when": { "param": "speed", "above": 0.3 }, "rate": { "param": "speed", "per": 1.8 } },
    { "name": "idle",   "clip": "idle" }
  ]
}
```

| Field | Meaning |
|---|---|
| `fade` | seconds to blend into a new state (0..2) |
| `name` | the state's name (unique) |
| `clip` | a clip of the entity's model by name, or `"models/other.glb#name"` from another file with the same joint names (AI motions: `"animations/wave.glb#<clip>"`) |
| `loop` | default `true`; `false` holds the last frame |
| `when.flag` | plays while this flag is on |
| `when.param` + `above` | plays while this number is above the value |
| `on` | a one-shot started by this trigger; it plays to its end |
| `rate.param` + `per` | playback speed = number / per, kept within 0.5..2 |
| `events` | `[{"at": 0.4, "name": "swing"}]`: times in the clip, for S9 (sounds, effects) |

A state without `when` or `on` always matches. **The first matching state wins**, so put the
important ones first: dead, then hit and attack, then movement, then idle last.

## What the game sets

| Name | Kind | Set by |
|---|---|---|
| `speed` | number, m/s | the player's and each enemy's horizontal speed, every frame |
| `attack` | trigger | a fighter that deals damage |
| `hit` | trigger | a fighter that takes damage |
| `dead` | flag | a defeated fighter; the enemy's body stays 2.5 s for its death clip |

A rigged player has no procedural bob and lean (`StrideAnimator`); the clips do the walking.

## Checks

`ParseAnimGraph` reports `animgraph.format`, `animgraph.fade`, `animgraph.states`,
`animgraph.state` (missing name/clip, duplicate name), `animgraph.when` (both or neither of
flag/param; `on` together with `when`), `animgraph.rate` and `animgraph.event`.
`BindAnimators` adds `entity.animation.graph` (file missing or invalid),
`entity.animation.skeleton` (the model isn't rigged) and `entity.animation.clip` (a state names a
clip the model doesn't have); the entity then keeps its rest pose. The game and editor show these
like other scene problems. Graph files are watched: saving one rebuilds the animators.
