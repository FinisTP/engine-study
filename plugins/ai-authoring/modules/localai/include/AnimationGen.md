# AI rigging and motion (`AnimationGen.hpp`, `ComfyUi.hpp` rig/motion part, S8)

Two jobs, both through a ComfyUI on this computer (like ModelGen.md: no cloud fallback):

```text
Rig:     models/elder.glb --upload--> the creator's exported rigging workflow (ComfyUI-UniRig)
         --> rigged .glb (or .fbx -> Blender -> .glb) --> models/elder_rigged.glb + .source.json
Motion:  "waves hello" (+ optional character) --> the exported text-to-motion workflow
         (ComfyUI-HY-Motion1) --> .glb (or .fbx -> .glb) --> animations/waves_hello.glb + .source.json
```

Editor: **AI animation** window. Tool:

```text
UmbraTool rig models/elder.glb --assets <folder> [--name <name>] [--workflow <api json>]
UmbraTool motion "waves hello" --assets <folder> [--character models/hero.glb] [--seconds 3]
```

## The creator's own workflows

As for 3D models, Umbra doesn't copy anyone's node graph. Build the workflow in ComfyUI, then
**Workflow -> Export (API)** into `<project>/tools/comfyui/`:

- `rig.api.json`: a mesh loader that names a model file (for example ComfyUI-UniRig's
  `UniRigLoadMesh` with `3d/character.glb`), the skeleton and skinning nodes, and a node that saves
  the rigged model. `LoadAnimWorkflow` wants **exactly one** input that names a `.glb`/`.fbx`/`.obj`
  file: Umbra uploads the model into that file's folder (`input/3d/`) and puts its name there.
- `motion.api.json`: ComfyUI-HY-Motion1's `Load LLM` -> `Encode Text` -> `Load Network` ->
  `Generate` -> `Export FBX` (or a node that saves GLB). Umbra fills every `text`/`prompt` input
  with the description, every `duration` with the length and every `seed` with this run's seed
  (+1 each). With a character, its file goes into `custom_fbx_path`: HY-Motion retargets the
  motion onto it (its exporter supports Mixamo-style bones).

Any other file name works; the window lists every usable export of the chosen kind.

## Which file is downloaded

ComfyUI 3D nodes report their files in different ways. `GenerateAnim` searches the run's outputs
for a `{filename, subfolder, type}` entry, or a path string, naming a `.glb` (preferred) or an
`.fbx`. A path on disk such as `C:\ComfyUI\output\rigged\x.glb` becomes subfolder `rigged`,
file `x.glb` of the output folder.

**FBX** (both packs' usual output) is converted with Blender in the background
(`FbxToGlb`: import FBX, export GLB with animations). Blender is found through `UMBRA_BLENDER`,
the PATH, or `C:\Program Files\Blender Foundation\*\blender.exe`. Without it the error says so;
a workflow that ends in a GLB-saving node needs no Blender.

## Saving and checks

`SaveAnimResult` writes new files only (`_2`, `_3`...) with a `.source.json` (workflow, source
model, prompt, seconds, seed, date), then imports the result with the game's importer: a rig must
have a skeleton and a motion at least one clip, or both files are removed (`model.rig`).

## Using the results

- A rigged model is used like Meshy's: `pack_animated.py` can add clips and normalise it, or use
  it as is with an `animation` component (the editor measures it at its rest pose).
- A motion's clip plays on any character with the same joint names (a `mixamorig:` prefix is
  ignored): `"clip": "animations/waves_hello.glb#<clip name>"` in a graph (AnimGraph.md). The
  window shows the clip names.

## Tested how

The cloud has no GPU for UniRig or HY-Motion, so `ComfyUiTests.cpp` runs both against a
stand-in ComfyUI with stand-in workflows (`tests/data/rig.api.json`, `motion.api.json`, using the
packs' node names) and a rigged model from the repository as the "result": upload folder and
name, filled-in inputs, the picked output, saving, and refusing a result without a skeleton. The
real node packs' output formats should be checked on Windows (testing/s8-skeletal-animation.md).

## Licenses

Check before shipping a game: UniRig (code MIT; check the weights' terms), HY-Motion 1.0 (its own
Tencent HY-Motion 1.0 Community License: commercial use is allowed except for very large
platforms; read it for the territory and acceptable-use terms), Make-It-Animatable (if the
workflow uses it). The `.source.json` names the workflow so this can be traced later.
