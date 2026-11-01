# Safe reload of models and scenes

Goal: edit a `.gltf`, its `.bin`, or a `.scene.json` while the game runs and see the change without restarting. A broken or half-saved file must **never** leave the running game in a mixed or crashed state.

## 1. Noticing a change: `FileWatcher`

Every 250 ms the app polls the watched files: the scene file, plus each loaded model's source and dependency files (`ModelLibrary::WatchedFiles`).

```text
poll:  stamp = (last write time, size, exists)
       stamp != seen      -> seen = stamp, since = now        (still changing: restart timer)
       seen != reported
         and now - since >= settle (250 ms)  -> report file   (stable: hand it over)
```

Editors often save in several steps (truncate, write, rename). The settle time makes sure the file has *stopped* changing before we read it. A file that is still unparsable after settling (truly broken) is handled by step 2.

## 2. Reloading a model: build next to the old one, then publish

```text
BuildModel(source)            fails? -> log, keep everything as it was          (T02)
ReadModelFile(new .umodel)    fails? -> log, keep everything
upload ALL new meshes/textures as NEW handles
                              any upload fails? -> destroy the new ones, keep the old
publish:   replace the library entry, destroy the old handles
           rebind every renderable in the world to the new handles
           (all before the next frame is recorded)
```

The old and new versions never mix. Until "publish" every frame draws the old version; after it, every frame draws the new one. The old handles are destroyed through the renderer's retire queue, so their GPU memory stays alive until in-flight frames finish (`engine/render/src/ResourceLifetime.md`, T44). That's why we create new handles instead of overwriting buffers in place.

`BuildModel` itself keeps the last good `.umodel` on failure (atomic temp-file + rename), so even a restart after a bad edit loads the last good version.

## 3. Content edit or structure edit?

`assets::SameStructure(old, new)` compares node names, parents and mesh lists.

| Change | Example | What happens |
|---|---|---|
| Content | Vertex positions, colors, textures, node transforms in the glTF | **Updated**: rebind handles. Entities keep their scene state. |
| Structure | Node added, removed, renamed or re-parented | **StructureChanged**: the scene is reloaded so `instance` entities are rebuilt from the new hierarchy. |

Node *transforms* inside an instanced model only take effect on the next scene reload, because instance entities copied them at load. Saving the scene file triggers that reload.

## 4. Reloading the scene

The scene file is loaded into a **new** `World` (validation first, see `engine/scene/include/SceneFormat.md`). Only if that fully succeeds is it swapped with the live world. On failure the old world stays, and its handles are rebound in case a model changed underneath it.

## Limits (by design, for now)

- Polling, not OS change notifications. That's simple, portable and plenty for a handful of files.
- Uploads are synchronous; a big model reload may hitch a frame.
- Runtime state that isn't in the scene file (e.g. the sample's spinning angle) resets on a scene reload. Gameplay state preservation across reloads is a later milestone (checkpoints, P4.4).
