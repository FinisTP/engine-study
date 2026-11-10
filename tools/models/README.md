# Model tools

`pack_animated.py` (Python 3 with `numpy` and `Pillow`) prepares a rigged character for Umbra (S8):

```text
python pack_animated.py rigged.glb out.glb [--turn 0] [--texture 1024]
       [--clip idle=idle.glb] [--clip walk=walk.glb#0] [--keep run=Gallop ...]
```

1. A new root node `UmbraRoot` turns the model by `--turn` degrees about Y (Umbra faces −Z;
   Meshy rigs already do, the Quaternius wolf needed 180), centers it and scales its largest
   half-extent to 1, measured on the skinned mesh at its rest pose.
2. All primitives become one skinned mesh. An untextured model gets a small palette texture made
   from its material colours.
3. Clips come from the file itself (`--keep new=old`) or from other GLBs with the same joint names
   (`--clip name=file.glb` or `file.glb#index`).
4. The base-colour texture becomes a JPEG of at most `--texture` pixels.

The engine's side (skins, clips, skinning math) is in `engine/core/include/Skeleton.md`.
Commands used for the reference characters: `tools/meshy/README.md`.
