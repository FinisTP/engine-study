# Scene file format (`*.scene.json`, version 1)

A scene is a readable JSON file that people, tools and AI agents can edit. It stores only **durable** data: ids, hierarchy, transforms and asset references. It never stores runtime handles, GPU objects or pointers.

```json
{
  "format": "umbra.scene",
  "version": 1,
  "entities": [
    { "id": "spinner", "name": "Spinner", "position": [0, 0, -2], "scale": [0.6, 0.6, 0.6],
      "model": "scenes/phase2.gltf", "mesh": 0 },
    { "id": "moon", "parent": "spinner", "position": [2, 0, 0], "color": [1, 0.8, 0.4, 1],
      "model": "scenes/phase2.gltf", "mesh": 0 },
    { "id": "tower", "instance": "models/tower.gltf" }
  ]
}
```

## Entity fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| `id` | string, required | | Durable id, unique in the file. Other entities and save files refer to it. |
| `name` | string | `""` | Display name. |
| `parent` | string | none | `id` of the parent entity (may appear later in the file). |
| `position` | `[x, y, z]` | `[0,0,0]` | Local position, meters (see `engine/world/include/Transform.md`). |
| `rotation` | `[x, y, z, w]` | `[0,0,0,1]` | Local rotation quaternion. Normalized on load. |
| `scale` | `[x, y, z]` | `[1,1,1]` | Local scale. |
| `model` | asset path | none | glTF file to draw from. Makes the entity renderable. |
| `mesh` | integer ≥ 0 | `0` | Which mesh (primitive) of `model`, in import order. |
| `color` | `[r, g, b, a]` | `[1,1,1,1]` | Tint multiplied with the material color. |
| `emissive` | number 0..100 | `0` | The surface's own glow, added to the light that reaches it: 1 shows the texture unlit, 2-3 glows (flames, crystals). It does not light other things; add a `light` for that (`engine/core/include/Lighting.md`). |
| `material` | asset path | none | A `*.material.json` whose image replaces the model's texture, optionally repeating by meters (`engine/assets/include/Material.md`). Needs `model`. |
| `instance` | asset path | none | At load, the whole node hierarchy of this glTF is created under this entity (ids `"<id>/<node name>"`). |
| `components` | object | none | `{"<type>": {fields}}`, one of each type: the built-in `collider`, `interactable`, `portal`, `spawner`, `light`, `environment`, `post`, `animation` (below) and the project's own (`Components.md`). |

Older files put the four built-in components directly on the entity (`"collider": {}`). They
still load; the editor moves them into `components` when it opens the file, and `SaveScene`
always writes `components`. Having the same type in both places is an error.

### Collider

```json
"collider": { "shape": "box", "motion": "dynamic", "halfExtents": [0.5, 0.5, 0.5] }
```

| Field | Values | Default |
|---|---|---|
| `shape` | `"box"`, `"sphere"`, `"capsule"` (vertical) | `"box"` |
| `motion` | `"static"` (level), `"kinematic"` (moved by code), `"dynamic"` (simulated) | `"static"` |
| `halfExtents` | `[x, y, z]`, box only | the entity's world scale (the sample cube spans -1..1) |
| `radius`, `halfHeight` | numbers, sphere/capsule | derived from the world scale |

Dynamic colliders must be on root entities, because the simulation owns their transform. `"collider": {}` is a static box matching the entity's scale.

### Interactable

```json
"interactable": { "prompt": "Read sign", "message": "Welcome to the settlement.", "sound": "audio/interact.wav" }
```

| Field | Meaning |
|---|---|
| `prompt` (required) | Shown as `[E] <prompt>` while the player looks at it within reach |
| `message` | Shown for a few seconds after using it |
| `sound` | WAV played on use (format rules: `engine/audio/include/Audio.md`) |

The player finds interactables with a short ray from the waist, so the entity also needs a `collider`.

### Portal

```json
"portal": { "scene": "scenes/dungeon.scene.json", "spawn": "entry" }
```

The portal's area is its transform applied to the -1..1 cube (exactly the box the sample cube
mesh draws), so give it a `model` to make it visible and **no** `collider` (the player has to be
able to walk into it). When the player's center enters the box, the app loads `scene` and puts
the player at the entity with id `spawn` (default `player`): its position is the feet, its
rotation the facing. Any entity can be a spawn marker; it needs no model.

A portal only fires after the player has been outside every portal since arriving, so a spawn
placed inside a portal does not bounce straight back. Place spawns a little away from portals.

### Light

```json
"light": { "color": [1, 0.62, 0.3], "intensity": 4, "range": 7, "flicker": 0.6 }
```

A point light at the entity's position. `color` is linear RGB, `intensity` 0..50 multiplies it,
`range` 0.1..100 m is where it fades to nothing, and `flicker` 0..1 makes it waver like a flame.
At most 16 lights are drawn at once: the ones nearest the camera (`Lighting.md`).

### Environment

```json
"environment": { "hour": 18.3, "dayLength": 720, "sunHeading": 30, "indoor": false,
                 "ambient": 1, "fog": 0.35, "tint": [1, 1, 1], "shadows": true }
```

One per scene (a second one gives the warning `scene.environment.twice` and is ignored), usually on an
empty entity named `sky`. Without one, the scene gets the defaults: 14:00, no clock, light fog.

| Field | Range | Meaning |
|---|---|---|
| `hour` | 0..24 | Time of day when the scene loads. `SaveScene` writes the current time. |
| `dayLength` | 0..86400 s | Real seconds per game day (720 = 12 minutes); 0 stops the clock. While the clock runs, the time carries on across areas and is kept in save games. |
| `sunHeading` | 0..360° | Which way the sun stands at noon. |
| `indoor` | bool | No sun, sky or sun shadows; a faint tinted light so torches do the work. |
| `ambient` | 0..4 | Strength of sky and ground light. |
| `fog` | 0..1 | Fog thickness (0.3 light haze, 1 thick). |
| `tint` | color | Multiplies all sky light (green swamp, red hell). |
| `shadows` | bool | Sun shadows. |

### Post

```json
"post": { "exposure": 0.1, "bloom": 0.7, "bloomThreshold": 1, "contrast": 1.08, "saturation": 1.1,
          "temperature": 0.35, "vignette": 0.3, "lut": "" }
```

How the scene's picture is finished (`engine/core/include/Post.md`). It works like `environment`:
one per scene (a second one gives the warning `scene.post.twice`), and without one the defaults apply.

| Field | Range | Meaning |
|---|---|---|
| `exposure` | -4..4 | Stops: +1 is twice as bright. |
| `bloom` | 0..2 | How much bright light glows around itself. |
| `bloomThreshold` | 0..10 | Light brighter than this blooms. |
| `contrast` | 0.5..2 | 1 = as is. |
| `saturation` | 0..2 | 0 = gray, 1 = as is. |
| `temperature` | -1..1 | Cold (blue) to warm (orange). |
| `vignette` | 0..1 | Darker corners. |
| `lut` | asset | Optional 256x16 color table image (`textures/`); a missing or wrong-sized one is `entity.post.lut`. |

### Animation

```json
"animation": { "graph": "animations/humanoid.anim.json" }
```

The entity's model must be rigged (a glTF skin); the graph picks its clips
(`engine/scene/include/AnimGraph.md`). An empty graph is `entity.animation.graph`. Problems found
when the game binds it (`entity.animation.skeleton`, `entity.animation.clip`, a bad graph file)
are listed with the scene's problems; the entity then stands in its rest pose.

Asset paths are relative to the asset folder and may not contain `..`.

**Reserved id:** the app treats the entity with id `player` as the player's spawn point and body (see `game/gameplay/include/PlayerController.md`).

## Loading is all-or-nothing

```text
read (≤ 64 MiB) → parse JSON → check header → check every entity → check hierarchy → load instance models
      any error: stop, report, touch nothing
      no errors: create entities → set parents → expand instances  (cannot fail any more)
```

`LoadScene` requires an empty world. The app loads into a **new** `World` and swaps it in only on success, so a broken file never half-changes the scene you're looking at.

## Diagnostics

Every problem is a `core::Diagnostic` (`engine/core/include/Diagnostic.hpp`) with a stable `code`, the `file`, a `field` path such as `entities[3].parent`, and a message.

| Code | Severity | Cause |
|---|---|---|
| `scene.read`, `scene.limit` | error | Unreadable file, more than 64 MiB, more than 100 000 entities |
| `scene.parse` | error | Invalid JSON (message includes the byte position) |
| `scene.format`, `scene.version`, `scene.entities` | error | Wrong header or unsupported version |
| `entity.id.missing`, `entity.id.duplicate` | error | Missing or repeated id (names the first use) |
| `entity.parent.missing` | error | `parent` names no entity |
| `entity.parent.cycle` | error | Parents loop; the message lists the chain, e.g. `a -> b -> a` |
| `entity.field.type`, `entity.field.nonfinite`, `entity.rotation.zero`, `entity.mesh.nomodel` | error | Wrong type, NaN/Inf, zero quaternion, `mesh` without `model` |
| `asset.path.unsafe`, `asset.missing` | error | Path escapes the asset folder, or the file doesn't exist |
| `instance.failed` | error | An `instance` model couldn't be loaded |
| `entity.material.nomodel`, `material.*` | error | `material` without `model`; the material file or its image is invalid (message names the material file and its field) |
| `entity.collider.shape`, `entity.collider.motion`, `entity.collider.size` | error | Unknown shape/motion or negative size |
| `entity.collider.dynamicchild` | error | Dynamic collider on an entity with a parent |
| `entity.interactable.prompt` | error | `interactable` without a prompt |
| `entity.interactable.sound` | warning | Sound file doesn't exist (the game plays nothing) |
| `entity.interactable.nocollider` | warning | `interactable` without `collider` (the player can't reach it) |
| `entity.portal.scene`, `entity.portal.spawn` | error | Portal without a destination, or an empty spawn id |
| `entity.component.twice` | error | A built-in component both on the entity and in `components` |
| `entity.component.field` | error | A project component's field has the wrong kind or is out of range (its schema) |
| `entity.component.unknown`, `entity.component.unknown-field` | warning | No `components/<type>.component.json` in the project; a field its schema doesn't list |
| `component.schema` | warning | A `*.component.json` is invalid; its components are not checked |
| `scene.environment.twice` | warning | A second `environment` component; only the first is used |
| `scene.post.twice` | warning | A second `post` component; only the first is used |
| `entity.post.lut` | error | The `post` component's LUT image is missing, unreadable or not 256x16 |
| `entity.animation.graph` | error | The `animation` component has no graph, or its graph file can't be used |
| `entity.animation.skeleton`, `entity.animation.clip` | error | (when the game binds animators) the model isn't rigged, or a state names a missing clip |
| `entity.field.unknown` | warning | Unknown field (usually a typo) |

## Saving

`SaveScene` writes roots (in slot order), each followed by its subtree, so parents always come before children. Only non-default values are written, rounded to 6 significant digits for readability. The write is atomic (temp file + rename). Instances are written **expanded**: every generated node becomes an explicit entity, so save → load reproduces the same world exactly.

## Checkpoints (development)

`F5` saves, `F9` restores, `R` restarts the area from its file. A checkpoint
(`engine/scene/include/Checkpoint.hpp`) is two files in the build's `asset-cache/checkpoint/`:

| File | Content |
|---|---|
| `world.scene.json` | The live world written by `SaveScene` (crates where physics left them, instances expanded) |
| `checkpoint.json` | `{"format":"umbra.checkpoint","version":1,"scene":<area path>,"feet":[x,y,z],"yaw":<radians>}` |

Restoring validates both like any scene and replaces nothing on error. Because instances are
saved expanded, a restored checkpoint saved again is byte-identical (tested), so repeated
save/restore cannot grow the world.

## Versioning

Readers accept exactly `version: 1`. A future format change bumps the number and adds an explicit migration. A file from a newer build is refused with a clear message rather than misread.
