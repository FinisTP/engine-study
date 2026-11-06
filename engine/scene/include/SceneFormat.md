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
| `instance` | asset path | none | At load, the whole node hierarchy of this glTF is created under this entity (ids `"<id>/<node name>"`). |
| `collider` | object | none | Physics shape, see below. |
| `interactable` | object | none | Something the player can use with **E**, see below. |

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

Asset paths are relative to the asset folder and may not contain `..`.

**Reserved id:** the app treats the entity with id `player` as the player's spawn point and body (see `engine/gameplay/include/PlayerController.md`).

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
| `entity.collider.shape`, `entity.collider.motion`, `entity.collider.size` | error | Unknown shape/motion or negative size |
| `entity.collider.dynamicchild` | error | Dynamic collider on an entity with a parent |
| `entity.interactable.prompt` | error | `interactable` without a prompt |
| `entity.interactable.sound` | warning | Sound file doesn't exist (the game plays nothing) |
| `entity.interactable.nocollider` | warning | `interactable` without `collider` (the player can't reach it) |
| `entity.field.unknown` | warning | Unknown field (usually a typo) |

## Saving

`SaveScene` writes roots (in slot order), each followed by its subtree, so parents always come before children. Only non-default values are written, rounded to 6 significant digits for readability. The write is atomic (temp file + rename). Instances are written **expanded**: every generated node becomes an explicit entity, so save → load reproduces the same world exactly.

## Versioning

Readers accept exactly `version: 1`. A future format change bumps the number and adds an explicit migration. A file from a newer build is refused with a clear message rather than misread.
