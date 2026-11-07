# Umbra Editor

`UmbraEditor [scene]` edits scenes and the RPG content of the reference game. It is a separate
program; the game (`Umbra`) links none of it (`Umbra.GameExcludesEditor` checks this).

## How it works

- **Documents, not worlds.** The open scene and `assets/rpg/reference.rpg.json` are
  `umbra::authoring::Document`s (`engine/authoring/include/Authoring.md`). Every change goes through
  `Document::Apply`. It is checked by the same loader the game uses, so the editor cannot save a
  file the game would refuse. A refused edit changes nothing, and the reason shows in
  **Diagnostics** and the status bar.
- **Preview.** After each change (edit, undo, redo) the preview world is rebuilt from the
  document text with `scene::LoadSceneText` and drawn by the game's renderer. Scenes are small, so
  a full rebuild is simpler than patching the world and takes well under a frame.
- **Shared code.** `ModelLibrary` (model file → GPU meshes) is compiled from `apps/umbra` into
  both programs, so there is one copy of it.

## Windows and keys

| Window | Use |
|---|---|
| Menu bar | File (New scene, Save, Reload, Quit), Edit (Undo, Redo, Add, Duplicate, Delete), View (RPG content), gizmo mode, **Play** |
| Hierarchy | Entities by parent. Click to select. Add / Add child. |
| Inspector | Id (renames children's references too), name, transform, model, collider, interactable, portal, enemy spawner. A checkbox adds or removes each optional part. |
| Assets | Scenes, RPG content, models, sounds under `assets/`. Double-click a scene to open it. |
| Diagnostics | The loader's messages for the last refused edit, open or save. |
| Status bar | File, `*` when unsaved, a warning when the file changed outside the editor. |

| Key / mouse | Action |
|---|---|
| Left click | Select what is under the mouse |
| Right drag / middle drag / wheel | Turn / pan / zoom the camera |
| W / E / R | Gizmo: move / rotate / scale |
| F | Focus the camera on the selection |
| Ctrl+Z / Ctrl+Y | Undo / redo. Undo follows the document you edited last. |
| Ctrl+S | Save the scene and the RPG content |
| Ctrl+D / Del | Duplicate / delete (with children) |
| F5 | Play: saves, then starts `Umbra <scene>` |

A whole gizmo drag, or a whole drag on a number field, is **one** undo step. The first change of a
drag starts a step; later changes merge into it (`Document::Apply(..., merge)`).

## RPG content forms (View → RPG content)

There is one tab per section: items, skills, statuses, actors, quests, conversations. Each tab
has the entry list (Add, Duplicate, Delete) on the left and the selected entry's form on the right.

| Section | Form |
|---|---|
| Items | slot, stack, modifiers while equipped, use effects |
| Skills | energy cost, target, turn/real-time modes (turning on real-time adds default timing), effects, timing, skill points, required skills |
| Statuses | turns, seconds, damage per tick, modifiers |
| Actors | stats and per-level growth, starting skills / equipment / inventory, slots, skill points, XP when defeated |
| Quests | description, ordered objectives (kind, target from the matching list, count, text), fail on death, reward |
| Conversations | NPC id, start node, nodes (speaker, line, choices with next node). `if` / `do` are JSON lists (format in `engine/story/include/Story.md`). |

The rules are the same as for scenes:

- Every field change is a validated, undoable edit of `reference.rpg.json`.
- Deleting or renaming something that other content still uses is refused, and the
  Diagnostics window names the reference (`ref.item`, `ref.skill`, ...).
- New entries start from a small template that already passes validation.
- Holding a number's +/- is one undo step.
- Ctrl+S saves the scene and the RPG content together.

The game reads the file when it starts (P7.5 adds reloading it while the game runs).

## AI quest draft (menu bar button, or View → AI quest draft)

Writes a side quest with a local AI model (Ollama, roadmap P8B). Nothing changes until you approve.

1. Pick the **quest giver** (NPCs that own a conversation), describe the **idea**, press
   **Write draft**. The request runs on a worker thread (`localai::ChatJob`); the window shows
   seconds and characters received, and **Cancel** stops it at once.
2. The answer is checked by `authoring::PlanDraft` (`engine/authoring/include/QuestDraft.md`):
   what will be added (preview), a play-through with the real story rules (green/red lines), and
   problems (red errors, yellow notes).
3. **Approve and add** (only without errors) is one undoable edit named `AI draft: <name>`. The
   RPG forms then open on the new quest for hand editing. Ctrl+Z undoes it; Ctrl+S saves it.
   **Ask the AI to fix the problems** sends the model its answer plus the problem list.
   **Write a new draft** starts over; **Discard** forgets the draft.
4. If the RPG content changed after the check (a form edit, undo), approve turns into
   **Check again**, so a draft never overwrites newer edits.

**Model** lists the models installed in Ollama (asked when the window opens; **Refresh** asks
again) and preselects the saved one, else an installed Qwen. Name and address can also be typed
under **Ollama settings**; both are saved in `ai-draft.json` in the preferences folder. Only
addresses on this computer are accepted.

## Materials and AI textures

The inspector's **Material** picker (under Model) lists `assets/materials/*.material.json`; the empty first entry
goes back to the model's own texture. Materials and textures also show in the asset browser.

**AI texture** (menu bar button, or View → AI texture; roadmap P8C):

1. On first open the window looks for ComfyUI on this computer (port 8000 = Desktop, 8188 =
   command line) and lists its checkpoints; an SDXL model is preselected. **Find** asks again.
2. Type what the surface looks like, set **meters per repeat** and the number of **variations**
   (1–4), press **Generate**. Pictures are made one after another on a worker thread
   (`localai::ImageJob`); **Cancel** stops at once and tells ComfyUI to stop too.
3. Each picture is resized to 1024 px, made seamless and scored (`TextureTools.md`). Picking a
   variation shows it **on the selected object in the scene view**, repeating at its real size.
   A seam score above 2 is marked "may show"; non-commercial or unknown model licenses are noted.
4. **Apply to object** writes `textures/<name>.png` and `materials/<name>.material.json` (new
   files, never overwritten: `_2`, `_3`...), then sets the object's `material` as one undoable
   edit. Undo removes the reference and keeps the files, so redo works. Without a selection the
   button is **Save material**. **Discard** forgets the variations.

Every prompt gets the project style from `assets/materials/style.json`.

## Picking (mouse → entity)

1. The mouse position becomes normalized device coordinates:
   `x = 2·mx/width − 1`, `y = 1 − 2·my/height` (screen y grows downward, NDC y upward).
2. With the inverse of `projection · view` (OpenGL-style projection, NDC z from −1 to 1), the
   points `(x, y, −1)` and `(x, y, 1)` are carried back to world space. After the divide by `w`
   they are points on the near and far planes. The ray runs from the near point toward the far one.
3. For every drawn entity the ray is moved into the entity's own space with the inverse world
   matrix. There the mesh's bounding box is axis-aligned, so a *slab test* is enough. Per axis,
   compute where the ray enters and leaves the pair of planes. The box is hit if the latest entry
   comes before the earliest exit. Because the direction is not normalized and both ends were
   transformed by the same matrix, the hit parameter `t` is comparable across entities. The
   smallest `t` wins.
4. Model instances create entities that aren't in the file, so the selection walks up to the
   nearest ancestor that is.

The renderer itself uses a Vulkan projection (depth 0..1, Y flipped). The editor builds the
OpenGL-style matrix only for ImGuizmo and picking, from the same camera.

## Gizmo

ImGuizmo edits the entity's **world** matrix. The editor turns the result back into the local
transform the file stores: `local = inverse(parentWorld) · world`. It then splits the matrix into
translation (column 3), scale (lengths of columns 0–2) and rotation (the columns divided by their
scale). The scene format has no shear, so nothing is lost.

## Limits

- Model files are not reloaded while the editor runs (the game reloads them). Reopen the scene
  after changing a model.
- Euler angles in the inspector are converted from the stored quaternion each frame. Past ±90°
  pitch they can jump to an equivalent rotation.
