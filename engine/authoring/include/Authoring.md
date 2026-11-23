# Authoring: the one way content gets edited

`Umbra::Authoring` is used by the editor (`apps/editor`), the command-line tool (`tools/umbratool`)
and, later, AI authoring tools (P8B). The game never links it.

## Documents

A `Document` is one content file (a scene, or the RPG content) held as JSON
(`nlohmann::ordered_json`, so keys keep their file order and a save doesn't reshuffle the file).

- `Open` reads the file and runs the **same validator the game uses to load it**
  (`scene::ValidateSceneText` or `rpg::ParseContent`). Only a valid file opens. That means every
  later state is valid too. A broken file is reported with the loader's diagnostics; fix it in a
  text editor, or with the tool's `validate` output as a guide.
- `Apply(changes, label)` applies a list of changes to a copy, validates the copy, and only then
  replaces the document. Either every change happens or none does.

## Changes

A `Change` is one RFC 6902 JSON Patch step at a JSON pointer:

| Op | Meaning | Example path |
|---|---|---|
| Set | replace a value, or add an object member | `/entities/3/position` |
| Insert | insert into an array before the index; `-` appends | `/items/-` |
| Remove | remove a value | `/entities/3` |

Changes apply in order, so a later change sees earlier ones. Example: add an item and a quest
that uses it in one edit.

`SceneEdits.hpp` builds common scene changes:

- **Remove entity**: the whole subtree, from the back so indices stay valid.
- **Rename entity**: also updates children's `parent`.
- **Set transform**, unique id, find by id.

## Undo, revisions, conflicts

- Each applied edit pushes the whole previous document on the undo stack (200 steps). Content
  files are small (tens of KB), so whole copies are simpler and safer than inverse patches.
- `merge = true` folds an edit into the previous step when the labels match. A gizmo drag sends
  an edit every frame but undoes in one step.
- `Revision()` names the current state. Every edit makes a new number; undo and redo go back to
  the number the state had.
  - **Dirty**: the revision differs from the saved revision.
  - **Conflict**: an edit may carry `expectedRevision`, the state it was prepared against. If the
    document is in a different state now, the edit is refused (`edit.conflict`). If undo brought
    the document back to exactly that state, the edit fits again.
- `Save` writes `file.tmp` and renames it over the file. If the file on disk changed since it was
  opened or saved (another program, or a hand edit), `Save` refuses (`save.conflict`) unless
  told to overwrite.

## Diagnostic codes added here

| Code | Meaning |
|---|---|
| `file.read`, `json.parse` | The file can't be read or isn't JSON |
| `edit.closed` | The document isn't open |
| `edit.path` | A change's JSON pointer doesn't fit the document |
| `edit.conflict` | The edit was prepared against another state |
| `save.conflict` | The file changed on disk since it was opened |
| `save.write` | Writing or replacing the file failed |

| `draft.*` | AI quest drafts: field, limit, kind, next, scope, play-through codes (`QuestDraft.md`) |

All loader codes (scene `entity.*`, RPG `ref.*`, etc.) come through unchanged.

## Package check (P9)

`CheckPackage` (`Package.hpp`) is the release-gate check of a packaged game folder; rules in `Package.md`.

## Play

`Play.hpp`: what the editor's Play (F5) can start. `projects/<project>/project.json` lists the starts:

```json
"play": [ { "label": "The Last Curtain: faction match", "program": "LastCurtain", "args": ["--match"] },
          { "label": "Umbra RPG: this scene", "program": "Umbra", "args": ["{scene}"] } ]
```

`program` is a name in the build's programs file (`out/build/<preset>/programs-<config>.json`, written
by `apps/editor/CMakeLists.txt` from `Umbra` and every plugin's `plugin.json` `"programs"`).
`PlayCommand` turns a start into the program's path and its arguments, `{scene}` filled. No starts:
the game on the open scene.
